#include "pugixml.hpp"
#include <iostream>
#include <map>
#include <unordered_map>
#include <unistd.h>
#include <set>
#include <filesystem>
#include "sqlwriter.hh"
using namespace std;

/* Every election has one or more KIESKRINGs. 

A KIESKRING has a candidate list, and different KIESKRINGs may have identical or different candidate lists.

Every KIESKRING is comprised of one or more GEMEENTES which run their own election infrastructure.

In the top-level VERKIEZINGSDEFINITIE we find the KIESKRINGS and the GEMEENTES.
We also find a list of RegisteredParty's (by name, not ID).

Per KIESKRING, we get KANDIDATENLIJSTEN which list the candidates fielded by the RegisteredParty's participating in that KIESKRING. Not all participate in all KIESKRINGS. A candidate also gets a numerical ID **which is valid only in the KIESKRING AND THE AFFILIATION/REGISTEREDPARTY**.

We also get a TELLING per GEMEENTE, and also a TELLING for the whole KIESKRING. These both refer to candidates by the KIESKRING AND PARTY SPECIFIC numerical ID. The KIESKRING file has the same semantics as the GEMEENTE file.

At the top-level of an election, next to the VERKIEZINGSDEFINITIE we also find the TOTAALTELLING. This includes two kinds of counts: the counts for the whole election, and the counts per KIESKRING. And here something interesting happens.

For the whole elections, candidates gain a new symbolic ID. We previously had a numerical ID for a candidate per AFFILIATION and per KIESKRING. But since parties need not have the same lists per KIESKRING, we can't use this numerical ID for the whole election.

To cover this up, the total results are keyed on a ShortCode, like 'HubertAWR'. The full name of the candidate is NOT included in the total election results (<TotalCounts>), only the key. And at that point, that ShortCode has not been defined anywhere.

Luckily the second part of the TOTAALTELLING includes aggregated counts per KIESKRING (in <ReportingUnitVotes>). And there the candidates are shown like this:

<ReportingUnitIdentifier Id="HSB1">Kieskring 's-Gravenhage</ReportingUnitIdentifier>
<Selection>
<AffiliationIdentifier Id="1">
<RegisteredName>Partyname</RegisteredName>
</AffiliationIdentifier>
<ValidVotes>6535</ValidVotes>
</Selection>
<Selection>
<Candidate>
<CandidateIdentifier Id="1" ShortCode="HubertAWR">
</CandidateIdentifier>
</Candidate>

To get to which candidate this is, go back to candidate with ID 1 within the list of the Affiliation with ID 1 within *THIS KIESKRING*. 
*/


struct Candidate
{
  int id{-1};
  string initials;
  string firstname;
  string prefix;
  string lastname;
  string locality;
  string gender;
  string shortcode;
  int ranking{-1};
  bool elected;

  string printName() const
  {
    string ret = initials;
    if(!prefix.empty()) 
      ret += " " +prefix;
    ret += " " +lastname;
    if(!firstname.empty())
      ret += " ("+firstname+")";
    return ret;
  }
};

// needs a <Candidate> node - note that almost everything is optional
Candidate parseCandidate(const pugi::xml_node& snode)
{
  if(((string)snode.name()) != "Candidate") {
    cerr<<"parseCandidate got passed wrong type "<<snode.name()<<endl;
    abort();
  }
  //  snode.print(std::cout);
  Candidate cand;
  /*
    <Candidate>
    <CandidateIdentifier Id="1" ShortCode="KegelEW"/>
    <CandidateFullName>
    <xnl:PersonName>
    <xnl:NameLine NameType="Initials">E.W.</xnl:NameLine>
    <xnl:FirstName/>
    <xnl:NamePrefix/>
    <xnl:LastName>Kegel</xnl:LastName>
    </xnl:PersonName>
    </CandidateFullName>
    <QualifyingAddress>
    <xal:Locality>
    <xal:LocalityName>Nootdorp</xal:LocalityName>
    </xal:Locality>
    </QualifyingAddress>
    </Candidate>
  */
  if(auto scattr = snode.child("CandidateIdentifier").attribute("ShortCode"))
    cand.shortcode = scattr.value();
  if(auto idattr = snode.child("CandidateIdentifier").attribute("Id")) {
    string tmp= idattr.value();
    if(!tmp.empty())
      cand.id = atoi(tmp.c_str());
  }
  else
    cand.id = -1;

  if(auto initnode = snode.child("CandidateFullName").child("xnl:PersonName").child("xnl:NameLine")) {
    if(initnode.begin() != initnode.end()) 
      cand.initials = initnode.begin()->value();
  }

  if(auto prefixnode = snode.child("CandidateFullName").child("xnl:PersonName").child("xnl:NamePrefix"))
    if(prefixnode.begin() != prefixnode.end())
      cand.prefix = prefixnode.begin()->value();

  if(auto gennode=snode.child("Gender"))
    cand.gender = gennode.begin()->value();

  if(auto fnnode = snode.child("CandidateFullName").child("xnl:PersonName").child("xnl:FirstName"))
    if(fnnode.begin() != fnnode.end())
      cand.firstname = fnnode.begin()->value();

  if(auto lnnode = snode.child("CandidateFullName").child("xnl:PersonName").child("xnl:LastName"))
    cand.lastname = lnnode.begin()->value();
  if(auto wp = snode.child("QualifyingAddress").child("xal:Locality").child("xal:LocalityName"))
    cand.locality = wp.begin()->value();
  
  return cand;
}


struct Affiliation
{
  int id;
  string name;
  bool elected;
};

// needs an <AffiliationIdentifier>
Affiliation parseAffiliation(const pugi::xml_node& snode)
{
  if(((string)snode.name()) != "AffiliationIdentifier") {
    cerr<<"parseAffiliation got passed wrong type "<<snode.name()<<endl;
    abort();
  }
 
  /*
    <AffiliationIdentifier Id="1">
    <RegisteredName>Forum voor Democratie</RegisteredName>
    </AffiliationIdentifier>
    <Elected>yes</Elected>
  */

  Affiliation aff;
  if(auto rnnode = snode.child("RegisteredName")) {
    if(rnnode.begin() != rnnode.end()) {
      aff.name = snode.child("RegisteredName").begin()->value();
    }
  }
  aff.id=atoi(snode.attribute("Id").value());

  if(auto elnode = snode.child("Elected")) {
    aff.elected = ((string)elnode.begin()->value()) == "yes";
    cout<<elnode.begin()->value();
  }
  
  return aff;
}


vector<string> findFiles(const std::string& p, vector<string>& in)
try
{
  //  cout<<"Starting at "<<p<<endl;
  for(const auto& entry : std::filesystem::directory_iterator(p)) {
    std::string filenameStr = entry.path().filename().string();
    //if the first found entry is directory go thru it
    if(entry.is_directory()) {
      //      std::cout << "Dir: " << filenameStr << '\n';
      findFiles(entry.path().string(), in);
    } 
    //print file name
    else if(entry.is_regular_file()) {
      //      std::cout << "file: " << filenameStr << ", "<<entry.path()<<endl;
      if(entry.path().extension().string() == ".xml")
        in.push_back(entry.path().string());
    }
  }
  return in;
}
 catch(std::exception &e)
   {
     in.push_back(p);
     return in;
   }

static int getSBId(const std::string& sb)
{
  if(auto pos =sb.find("::SB"); pos != string::npos) {
    return stoi(sb.substr(pos+4));
  }
  throw runtime_error("Could not convert stembureau id "+sb+": ::SB missing");
}

int main(int argc, char **argv)
{
  unlink("eml.sqlite");

  vector<string> files;
  for(int n=1; n< argc; ++n) 
    files = findFiles(argv[n], files);
  
  SQLiteWriter sqw("eml.sqlite");
  map<pair<int,int>, string> candnames;
  if(files.empty()) {
    cout<<"No files found\n";
    return 0;
  }
  string prevElectionId;
  for(const auto& filename : files) {
    pugi::xml_document doc;
    if (!doc.load_file(filename.c_str())) {
      cout<<"Could not load "<<filename<<endl;
      return -1;
    }
    string formid;
    if(auto emlnode = doc.child("EML")) {
      formid=emlnode.attribute("Id").value();
      cout<<"'"<<filename<<"' is EML form "<< formid <<endl;
    }
    else {
      cerr<<"'"<<filename<<" is not EML"<<endl;
      return -1;
    }

    string electionId;
    if(atoi(formid.c_str())==510) {
      electionId = doc.child("EML").child("Count").child("Election").child("ElectionIdentifier").attribute("Id").value();
    }
    else if(atoi(formid.c_str())==230) {
      electionId = doc.child("EML").child("CandidateList").child("Election").child("ElectionIdentifier").attribute("Id").value();
    }

    else if(formid == "520")
      electionId = doc.child("EML").child("Result").child("Election").child("ElectionIdentifier").attribute("Id").value();
    else
      electionId = doc.child("EML").child("ElectionEvent").child("Election").child("ElectionIdentifier").attribute("Id").value();
    if(electionId.empty()) {
      cerr<<"No election ID in formid '"<<formid<<"'"<<endl;
      return -1;
    }
    /*
    if(!prevElectionId.empty() && electionId != prevElectionId) {
      cerr<<"Only one election per database please! Was processing "<<prevElectionId<<", saw new election '"<<electionId<<"'"<<endl;
      return -1;
    }
    */
    prevElectionId = electionId;
    /* 
       formid: 110a -> Verkiezingsdefinitie
       Mentions the election tree, in which the kieskringen are denoted by their NON-ROMAN numeral if their number was roman

       formid: 230b -> Kandidatenlijst
       Note that this list is per kieskring, gemeente or waterschap.
       When as kieskring, the full name and ID are as follows:

       <ContestIdentifier Id="II">
       <ContestName>Venlo</ContestName>
       </ContestIdentifier>
       
       Note that the Id COULD BE IN ROMAN NUMERALS!!11!!

       For gemeente, there is no ContestIdentifierName. The gemeente name however is in <kr:ElectionDomain Id="1926">Pijnacker-Nootdorp</kr:ElectionDomain>.

       formid: 510b -> Telling (gemeente, first totals, then per reporting unit)
       formid: 510c -> telling kieskring (votes for kieskring + gemeentes)
       formid: 510d -> Totaaltelling provincie (ShortCode votes for provincie + Kieskringen with full name AND ShortCode)

       These three levels of counts partially overlap. The 510x forms all list the counts per candidate and affiliation at their own level, 
       and at the reporting units below.

       For a provincial election: 
         510d has the total counts, and 510d lists the kieskring totals as reporting units
         510c then has the total results of a kieskring, plus the totals reported by the gemeentes
         510b finally has the results for a gemeente, plus the individual counting stations as reporting units



                        <- 510d --------->
                                <--- 510c    ------>
                                            <--- 510b ---------->
         Provincie PS2: TOTAL / KIESKRING / GEMEENTE / STEMBUREAU

         510d here has kieskringen as reportingunits, which are called 'HSB1', 'HSB2' etc. The 1 matches the Id in 510C, HOWEVER, over there it could be in roman numbers
         510d 

         The KIESKRING name in 510c is repeated three times: AuthorityIdentifier, ElectionDomain, ContestName. 
         <AuthorityIdentifier Id="HSB1">Maastricht</AuthorityIdentifier>
         This Id matches up with the 510d in all cases

         An ID is also mentioned in the AuthorityIdentifier (HSB3) and in the ContestName (ContestIdentifier, number, 3).
         This number matches the ContestIdentifier in the kandidatenlijst 230b.

         HOWEVER! Note the RomanNumerals thing in Limburg

         510c refers to GEMEENTEs in two ways, by name and by HSB1::1729:
         <ReportingUnitIdentifier Id="HSB1::1729">Gulpen-Wittem</ReportingUnitIdentifier>

         510b ties back to the kieskring with the ContestIdentifier ID (retains roman numeral status) and ContestName.
         It does NOT contain the name of the province.
         510b has the GEMEENTE name in two places:
         <AuthorityIdentifier Id="1729">Gulpen-Wittem</AuthorityIdentifier>
         and
         <kr:ElectionDomain>Gulpen-Wittem</kr:ElectionDomain>



                       <-- 510d ------->
                               <- 510b ------------>
                   PS: TOTAL / GEMEENTE / STEMBUREAU ?

                   For PS without KIESKRING, 510b lists ContestIdentifier Id as "geen" (none), but the province name can be found in:
                   <kr:ElectionDomain>Flevoland</kr:ElectionDomain>


         Tweede Kamer: TOTAL / KIESKRING / GEMEENTE / STEMBUREAU

                       <-- 510b -------->         
         Gemeenteraad: TOTAL / STEMBUREAU
         Link to Kandidatenlijst is not really needed. However, from 510b, this:
         <kr:ElectionDomain Id="1926">Pijnacker-Nootdorp</kr:ElectionDomain>
         matches to: 
         <kr:ElectionDomain Id="1926">Pijnacker-Nootdorp</kr:ElectionDomain>
         in 
         230b

                                <-- 510b ----------->    
         Waterschappen: TOTAL / GEMEENTE / STEMBUREAU

       formid: 520 -> Resultaat (SEATS), with ShortCode
         You can have *one* of these per election.
         Gives the result in seats. 
         Lists full names and ShortCodes. There is also an Id that shows the order on the elected list, plus a Ranking. 
         A Ranking of 1 means this candidate was elected by preference votes. 
    */

    if(formid=="110a") { //  verkiezingsdefinitie
      auto start = doc.child("EML").child("ElectionEvent").child("Election");
      //
      int noSeats = 0;
      string electionName, electionDomain;
      string category, subcategory;
      for(const auto& node : start) {
        string name = node.name();
        if(name=="kr:ElectionTree") {
          for(const auto& n : node) {
            string name=n.name();
            if(name=="kr:Region" && string(n.attribute("RegionCategory").value())=="KIESKRING") {
              cout<<"KIESKRING: "<<n.child("kr:RegionName").begin()->value();
              cout<<" ("<<atoi(n.attribute("RegionNumber").value())<<")\n";
            }
          }
        }
        else if(name=="kr:RegisteredParties") {
          //          int orderno = 1;
          for(const auto& n : node) {
            string name=n.name();

            if(name=="kr:RegisteredParty") {
              string pname=n.child("kr:RegisteredAppellation").begin()->value();
              cout<<"Party: "<<pname<<endl;
              //              sqw.addValue({{"name", pname}, {"orderno", orderno}}, "parties");
              // the numbering here is .. different from the rest
            }
          }
        }
        else if(name=="kr:NumberOfSeats") {
          noSeats = atoi(node.begin()->value());
        }
        else if(name=="ElectionIdentifier") {
          electionName=node.child("ElectionName").begin()->value();
          if(auto domnode = node.child("kr:ElectionDomain"))
            if(domnode.begin() != domnode.end()) 
              electionDomain=domnode.begin()->value();
          if(auto catnode = node.child("ElectionCategory"))
            if(catnode.begin() != catnode.end()) 
              category=catnode.begin()->value();
          if(auto subcatnode = node.child("kr:ElectionSubcategory"))
            if(subcatnode.begin() != subcatnode.end()) 
              subcategory=subcatnode.begin()->value();

          
        }
        
        else
          cout << "VERKIEZINGSDEFINITIE: "<<node.name() << endl;
      }

      
      static map<string,string> pscodes{{"Noord-Holland", "PV27"},
                                        {"Zeeland", "PV29"},
                                        {"Groningen", "PV20"},
                                        {"Overijssel", "PV23"},
                                        {"Noord-Brabant", "PV30"},
                                        {"Gelderland", "PV25"},
                                        {"Zuid-Holland", "PV28"},
                                        {"Friesland", "PV21"},
                                        {"Fryslân","PV21"},
                                        {"Drenthe", "PV22"},
                                        {"Utrecht", "PV26"},
                                        {"Flevoland", "PV24"},
                                        {"Limburg", "PV31"}};

      static map<string,string> wscodes{{"Aa en Maas","W0654"},
                                        {"Amstel, Gooi en Vecht","W0155"},
                                        {"Brabantse Delta","W0652"},
                                        {"De Dommel","W0539"},
                                        {"De Stichtse Rijnlanden","W0636"},
                                        {"Delfland","W0372"},
                                        {"Drents Overijsselse Delta","W0664"},
                                        {"Fryslân","W0653"},
                                        {"Hollands Noorderkwartier","W0651"},
                                        {"Hollandse Delta","W0655"},
                                        {"Hunze en Aa's","W0646"},
                                        {"Limburg","W0665"},
                                        {"Noorderzijlvest","W0647"},
                                        {"Rijn en IJssel","W0152"},
                                        {"Rijnland","W0616"},
                                        {"Rivierenland","W0621"},
                                        {"Scheldestromen","W0661"},
                                        {"Schieland en de Krimpenerwaard","W0656"},
                                        {"Vallei en Veluwe","W0662"},
                                        {"Vechtstromen","W0663"},
                                        {"Zuiderzeeland","W0650"}
      };
      /*
Regio,RegioCode,OuderRegioCode
"Nederland","L528"
      */
      
      sqw.addValue({{"id", electionId}, {"name", electionName}, {"domain", electionDomain},
                    {"seats", noSeats}, {"category", category}, {"subcategory", subcategory},
                    {"code", category=="PS" ? pscodes[electionDomain] : wscodes[electionDomain]},
                    {"oudercode", category=="AB" ? "L528" : ""}}, "election");
    }
    else if(atoi(formid.c_str())==510) { // votes
      auto start = doc.child("EML").child("Count").child("Election");
      // this contains votes for the current and one level lower:
      // d -> main, c -> kieskring, b -> gemeente (-> a, counting station)
      // TOP     KIESKRING,    GEMEENTE,  STEMBUREAU

      string top("Provincie"), kieskringName, kieskringHSB,  gemeente, stembureau;
      int gemeenteId=-1;
      int kieskringId=-1;
      string subcategory, category;
      if(formid=="510d") {
        // nothing to set
        // need to detect of this is PS1 or PS2!
        // <kr:ElectionSubcategory>PS1</kr:ElectionSubcategory>
        /*
        <Election>
          <ElectionIdentifier Id="PS2023_Utrecht">
               <ElectionName>Provinciale Staten Utrecht 2023</ElectionName>
               <ElectionCategory>PS</ElectionCategory>
               <kr:ElectionSubcategory>PS1</kr:ElectionSubcategory>
        */
        category = start.child("ElectionIdentifier").child("ElectionCategory").begin()->value();
        if(auto subnode = start.child("ElectionIdentifier").child("kr:ElectionSubcategory"))
          subcategory = subnode.begin()->value();
        cout<<"Top level counts, subcategory = "<<subcategory<<endl;
        
      }
      else if(formid=="510c") { // kieskring counts
        //        <AuthorityIdentifier Id="HSB1">Maastricht</AuthorityIdentifier>
        kieskringName=doc.child("EML").child("ManagingAuthority").child("AuthorityIdentifier").begin()->value();
        kieskringHSB = doc.child("EML").child("ManagingAuthority").child("AuthorityIdentifier").attribute("Id").value();
        string tmp = start.child("Contests").child("Contest").child("ContestIdentifier").attribute("Id").value();
        if(tmp=="I") // limburg
          kieskringId = 1;
        else if(tmp=="II")
          kieskringId = 2;
        else if(tmp=="III")
          kieskringId = 3;
        else kieskringId = atoi(tmp.c_str());
      }
      else if(formid == "510b") { // gemeente counts
        if(auto kiesknode =start.child("Contests").child("Contest").child("ContestIdentifier").child("ContestName"))
          kieskringName = kiesknode.begin()->value();
        string tmp = start.child("Contests").child("Contest").child("ContestIdentifier").attribute("Id").value();
        if(tmp=="I") // limburg
          kieskringId = 1;
        else if(tmp=="II")
          kieskringId = 2;
        else if(tmp=="III")
          kieskringId = 3;
        else kieskringId = atoi(tmp.c_str());
        kieskringHSB = "HSB"+to_string(kieskringId);

        gemeente=doc.child("EML").child("ManagingAuthority").child("AuthorityIdentifier").begin()->value();
        gemeenteId = stoi(doc.child("EML").child("ManagingAuthority").child("AuthorityIdentifier").attribute("Id").value());
      }

      //      cout<<"Form "<<formid<<": kieskringName '"<<kieskringName<<"' kieskringHSB '"<< kieskringHSB<<"' kieskringId "<<kieskringId<<endl;
      auto sels = start.child("Contests").child("Contest").child("TotalVotes");
      Affiliation aff;
      int orderno=1;
      for(const auto& s : sels) {
        string sname = s.name();

        if(sname=="Selection") {
          string sc;
          bool isAffiliation=false;
          Candidate cand;

          for(const auto& snode : s) {
            string snname = snode.name();
            if(snname=="AffiliationIdentifier") {
              aff = parseAffiliation(snode);
              isAffiliation = true;
              orderno=1;
            }
            else if(snname=="Candidate") {
              cand = parseCandidate(snode);
              isAffiliation=false;
            }
            else if(snname=="ValidVotes") {
              if(!isAffiliation) {
                if(cand.id < 0)
                  sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"formid", formid}, {"gemeente", gemeente},
                                {"gemeenteId", gemeenteId},
                                {"affid", aff.id}, {"shortcode", cand.shortcode}, {"orderno", orderno}, {"votes", atoi(snode.begin()->value())}}, "candvotecounts");
                else
                  sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"formid", formid}, {"gemeente", gemeente},
                                {"gemeenteId", gemeenteId},
                                {"affid", aff.id}, {"candid", cand.id}, {"orderno", cand.id}, {"votes", atoi(snode.begin()->value())}}, "candvotecounts");
                // repeat orderno here for consistency, even though we don't need it for PS1 elections
                orderno++;
              }
              else {
                  sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"formid", formid}, {"gemeente", gemeente},
                                {"gemeenteId", gemeenteId},
                                {"affid", aff.id}, {"votes", atoi(snode.begin()->value())}}, "affvotecounts");
                
              }
            }
            else
              cout<<"Unknown snname in 510 TotalVotes "<<snname<<endl;

          }
        } // what follows is a stupid copy from 'rumeta' from the reporting units
        else if(sname=="Cast") {
          //          cout<<"  total ballots "<<s.begin()->value()<<endl;
          sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId},
                        {"formid", formid}, {"gemeente", gemeente},
                        {"gemeenteId", gemeenteId},
                        {"kind", "totalballots"}, {"value", atoi(s.begin()->value())}}, "meta");
          
        }
        else if(sname=="TotalCounted") {
          //          cout<<"  totalcounted "<<s.begin()->value()<<endl;
          sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId},
                        {"formid", formid}, {"gemeente", gemeente},
                        {"gemeenteId", gemeenteId},
                        {"kind", "totalcounted"}, {"value", atoi(s.begin()->value())}}, "meta");
          
        }
        else if(sname=="RejectedVotes") {
          string reason = s.attribute("ReasonCode").value();
          sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId},
                        {"formid", formid}, {"gemeente", gemeente},
                        {"gemeenteId", gemeenteId},
                        {"category", sname},{"kind", reason}, {"value", atoi(s.begin()->value())}}, "meta");
          
          //          cout<<"  rejectedvotes "<<s.begin()->value()<<", reason: "<<reason<<endl;
        }
        else if(sname=="UncountedVotes") { // this is METADATA
          string reason = s.attribute("ReasonCode").value();
          sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId},
                        {"formid", formid}, {"gemeente", gemeente},
                        {"gemeenteId", gemeenteId},
                        {"category", sname},{"kind", reason}, {"value", atoi(s.begin()->value())}}, "meta");
          
          //          cout<<"  uncountedvotes "<<s.begin()->value()<<", reason: "<<reason<<endl;
        }
      
      }
      for(const auto&s : start.child("Contests").child("Contest")) {
        string name = s.name();
        if(name == "ReportingUnitVotes") {
          int affid=-1;
          string affname;
          string runame, ruId;
          string stembureau, postcode;
          int stembureauId=-1;
          for(const auto& s2 : s) {
            string lname = s2.name();
            if(lname=="ReportingUnitIdentifier") {
              // <ReportingUnitIdentifier Id="HSB1">Kieskring Maastricht</ReportingUnitIdentifier>
              runame = s2.begin()->value();
              ruId = s2.attribute("Id").value();
              //              cout<<"Counts from ReportingUnit: "<<runame<<endl;
            }
            else if(lname=="Selection") {
              int validvotes = atoi(s2.child("ValidVotes").begin()->value());

              if(formid=="510b") {
                // <ReportingUnitIdentifier Id="0965::SB1">Stembureau Gemeentehuis (postcode: 6369 AH)</ReportingUnitIdentifier>
                stembureau = runame;
                stembureauId = getSBId(ruId); // strips 0965::
                if(auto pcodepos = stembureau.find("(postcode: "); pcodepos != string::npos) {
                  auto epos = stembureau.find(")", pcodepos);

                  if(epos != string::npos)
                    postcode = stembureau.substr(pcodepos+11, epos - pcodepos - 11);

                  if(postcode.length()==7 && postcode.at(4)==' ') {
                    postcode[4]=postcode[5];
                    postcode[5]=postcode[6];
                    postcode.resize(6);
                  }
                }
              }
              else if(formid=="510c") {
                gemeente = runame;
              }
              else if(formid == "510d") {
                // <ReportingUnitIdentifier Id="HSB1">Kieskring Maastricht</ReportingUnitIdentifier> for PS2
                // or
                // ReportingUnitIdentifier Id="0307">Amersfoort</ReportingUnitIdentifier> for PS1
                if(category=="PS") {
                  if(subcategory=="PS2") {
                    kieskringName=runame;
                    kieskringHSB=ruId;
                    kieskringId = atoi(&ruId.at(3));
                  }
                  else if(subcategory =="PS1") {
                    kieskringName = "?";
                    gemeente = runame;
                    kieskringId = -1;
                  }
                }
                else if(category=="TK") {
                  kieskringName=runame;
                  kieskringHSB=ruId;
                  kieskringId = atoi(&ruId.at(3));
                }
                else if(category=="EK") {
                  kieskringName=runame;
                  kieskringHSB=ruId;
                  kieskringId = atoi(&ruId.at(3));
                }
                else if(category=="AB") {
                  kieskringName="?";
                  kieskringHSB="";
                  kieskringId = -1;
                  gemeente = runame;
                }
                else {
                  cerr<<"No idea how to do a "<<category<<" ("<<subcategory<<") election"<<endl;
                  cout<<"runame: "<<runame<<", ruid: "<< ruId<<endl;
                  exit(1);
                }
              }
              
              if(auto cand=s2.child("Candidate")) {
                int candid = atoi(cand.child("CandidateIdentifier").attribute("Id").value());
                string shortcode;
                if(auto sc=cand.child("CandidateIdentifier").attribute("ShortCode")) {
                  shortcode = sc.value();
                }

                static set<pair<int,int>> stembureauIds;
                // this saves a ton of space
                if(!stembureau.empty() && !stembureauIds.count({gemeenteId,stembureauId})) {
                  sqw.addValue({{"electionId", electionId},{"id", stembureauId}, {"name", stembureau}, {"gemeente", gemeente}, {"postcode", postcode},
                    {"gemeenteId", gemeenteId}}, "stembureaus");
                  stembureauIds.insert({gemeenteId,stembureauId});
                }
                
                sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"formid", formid}, {"gemeente", gemeente},
                              {"gemeenteId", gemeenteId},
                              {"stembureauId", stembureauId}, {"affid", affid}, {"candid", candid}, {"shortcode", shortcode}, {"votes", validvotes}}, "rucandvotecounts");
              }
              else if(auto aff=s2.child("AffiliationIdentifier")) {
                affid = atoi(aff.attribute("Id").value());
                if(aff.child("RegisteredName").begin() != aff.child("RegisteredName").end())
                  affname = aff.child("RegisteredName").begin()->value();
                else
                  affname="";

                // XXX we should save a lot of space here and only log the stembureauId, and have a separate table with the info
                sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"formid", formid}, {"gemeente", gemeente},
                              {"gemeenteId", gemeenteId},
                              {"stembureau", stembureau},  {"stembureauId", stembureauId}, {"postcode", postcode},{"affid", affid}, {"votes", validvotes}}, "ruaffvotecounts");

              }
            }
            else if(lname=="Cast") {
              //              cout<<"  total ballots "<<s2.begin()->value()<<endl;
              sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId},
                            {"formid", formid}, {"gemeente", gemeente},
                            {"gemeenteId", gemeenteId},
                            {"stembureau", stembureau},
                            {"stembureauId", stembureauId},
                            {"postcode", postcode},{"kind", "totalballots"}, {"value", atoi(s2.begin()->value())}}, "rumeta");

            }
            else if(lname=="TotalCounted") {
              //              cout<<"  totalcounted "<<s2.begin()->value()<<endl;
              sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId},
                            {"formid", formid}, {"gemeente", gemeente},
                            {"gemeenteId", gemeenteId},
                            {"stembureau", stembureau},
                            {"stembureauId", stembureauId},
                            {"postcode", postcode},{"kind", "totalcounted"}, {"value", atoi(s2.begin()->value())}}, "rumeta");

            }
            else if(lname=="RejectedVotes") {
              string reason = s2.attribute("ReasonCode").value();
              sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId},
                            {"formid", formid}, {"gemeente", gemeente},
                            {"gemeenteId", gemeenteId},
                            {"stembureau", stembureau},
                            {"stembureauId", stembureauId},
                            {"postcode", postcode},{"category", lname},{"kind", reason}, {"value", atoi(s2.begin()->value())}}, "rumeta");

              //              cout<<"  rejectedvotes "<<s2.begin()->value()<<", reason: "<<reason<<endl;
            }
            else if(lname=="UncountedVotes") { // this is METADATA
              string reason = s2.attribute("ReasonCode").value();
                            sqw.addValue({{"electionId", electionId},{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId},
                            {"formid", formid}, {"gemeente", gemeente},
                            {"gemeenteId", gemeenteId},
                            {"stembureau", stembureau},
                            {"stembureauId", stembureauId},
                            {"postcode", postcode},{"category", lname},{"kind", reason}, {"value", atoi(s2.begin()->value())}}, "rumeta");

                            //              cout<<"  uncountedvotes "<<s2.begin()->value()<<", reason: "<<reason<<endl;
            }
            else
              cout<<"Unknown 510 field in ReportingUnit: '"<<lname<<"'"<<endl;

            /*
<Cast>459490</Cast>
<TotalCounted>193783</TotalCounted>
<RejectedVotes ReasonCode="ongeldig">884</RejectedVotes>
<RejectedVotes ReasonCode="blanco">788</RejectedVotes>
<UncountedVotes ReasonCode="geldige volmachtbewijzen">20284</UncountedVotes>
<UncountedVotes ReasonCode="geldige kiezerspassen">90</UncountedVotes>
<UncountedVotes ReasonCode="meer getelde stembiljetten">109</UncountedVotes>
<UncountedVotes ReasonCode="minder getelde stembiljetten">200</UncountedVotes>
<UncountedVotes ReasonCode="meegenomen stembiljetten">19</UncountedVotes>
<UncountedVotes ReasonCode="te weinig uitgereikte stembiljetten">3</UncountedVotes>
<UncountedVotes ReasonCode="te veel uitgereikte stembiljetten">7</UncountedVotes>
<UncountedVotes ReasonCode="geen verklaring">249</UncountedVotes>
<UncountedVotes ReasonCode="andere verklaring">25</UncountedVotes>
            */
          }
        }
      }
    }
    else if(formid=="230b") {
      auto start=doc.child("EML").child("CandidateList").child("Election").child("Contest");
      cout<<"Candidate list"<<endl;
      string top("Provincie"), kieskringName, kieskringHSB,  gemeente, stembureau;
      int kieskringId=-1;

      if(auto kiesknode = start.child("ContestIdentifier").child("ContestName"))
        kieskringName = kiesknode.begin()->value();
      string tmp = start.child("ContestIdentifier").attribute("Id").value();
      if(tmp=="I") // limburg
        kieskringId = 1;
      else if(tmp=="II")
        kieskringId = 2;
      else if(tmp=="III")
        kieskringId = 3;
      else kieskringId = atoi(tmp.c_str());
      kieskringHSB = "HSB"+to_string(kieskringId);
      
      gemeente=doc.child("EML").child("ManagingAuthority").child("AuthorityIdentifier").begin()->value();

      cout<<"Candidates from KIESKRING '"<<kieskringName<<"', Id="<<kieskringId<<endl;
      
      for(const auto& node : start) {
        string name = node.name();
        
        if(name=="Affiliation") {
          auto aff = parseAffiliation(node.child("AffiliationIdentifier"));
          //          cout<<"Affiliation: "<<aff.name<< " ("<<aff.id<<")\n";
          sqw.addValue({ {"id", aff.id}, {"electionId", electionId},{"kieskringName", kieskringName}, {"kieskringHSB", kieskringHSB},
                         {"kieskringId", kieskringId}, {"name", aff.name}}, "affiliations");

          for(const auto& c : node) {
            string cname = c.name();
            if(cname=="Candidate") {
              auto cand = parseCandidate(c);
              //              cout << cand.printName()<< " (" <<cand.id <<")"<<endl;
              candnames[{aff.id, cand.id}]  = cand.initials + (cand.prefix.empty() ? "" : (" "+cand.prefix)) + " " + cand.lastname;
              sqw.addValue({{"electionId", electionId},{"kieskringName", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"id", cand.id}, {"affid", aff.id},
                            {"firstname", cand.firstname}, {"initials", cand.initials}, {"prefix", cand.prefix}, {"lastname", cand.lastname},
                            {"gender", cand.gender}, {"woonplaats", cand.locality}
                }, "candentries");
            }
            else if(cname=="Type") {
            }
            else if(cname=="kr:ListData") {
            }
            else if(cname=="AffiliationIdentifier") { // already parsed above
            }
            else
              cout<<"? "<<cname<<endl;
          }
        }
        else cout<<name<<endl;
      }
    }
    else if(formid=="520") {
      auto start = doc.child("EML").child("Result").child("Election").child("Contest");
      Affiliation aff;
      for(const auto& sel : start) {
        if((string)sel.name() != "Selection") {
          cerr<<"Unknown item in Results: "<<sel.name()<<endl;
          continue;
        }
        int ranking=-1;
        Candidate cand;
        if(const auto& snode= sel.child("AffiliationIdentifier")) {
          aff = parseAffiliation(snode);
          //          cout<<"Party name: "<<aff.name<< " ("<<aff.id<<")\n";
        }
        else if(const auto& snode= sel.child("Candidate")) {
          cand = parseCandidate(snode);
          //          cout<<"Candidate name: "<<cand.printName()<<", id "<<cand.id<<endl;
          ranking = atoi(sel.child("Ranking").begin()->value());
          //          cout<<" ranking: "<<ranking<<endl;
        }
        bool elected = ((string)sel.child("Elected").begin()->value())=="yes";
        /*
        <Ranking>1</Ranking>
          <Elected>yes</Elected>
        */
        //        cout<<" elected: "<<elected<<endl;
        if(ranking > 0)
          sqw.addValue({{"electionId", electionId},{"affid", aff.id}, {"resultorder", cand.id},
                        {"shortcode", cand.shortcode},
                        {"initials", cand.initials},
                        {"prefix", cand.prefix},
                        {"lastname", cand.lastname},
                        {"firstname", cand.firstname},
                        {"woonplaats", cand.locality},
                        {"gender", cand.gender},
                        {"elected", elected},
                        {"ranking", ranking}}, "candresults");
        else
          sqw.addValue({{"electionId", electionId},{"affid", aff.id}, {"name", aff.name}},
                       "affresults");
      }
    }
    else {
      cout<<"Unknown, top elements: "<<endl;
      for(const auto& e : doc) {
        cout<<e.name()<<endl;
      }
    }
  }
}

