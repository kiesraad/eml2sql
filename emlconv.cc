#include "pugixml.hpp"
#include <iostream>
#include <map>
#include <unordered_map>
#include <unistd.h>
#include <set>
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


int main(int argc, char **argv)
{
  unlink("eml.sqlite");
  SQLiteWriter sqw("eml.sqlite");
  map<pair<int,int>, string> candnames;
  for(int n=1; n< argc; ++n) {
    pugi::xml_document doc;
    if (!doc.load_file(argv[n])) {
      cout<<"Could not load "<<argv[n]<<endl;
      return -1;
    }
    string formid;
    if(auto emlnode = doc.child("EML")) {
      formid=emlnode.attribute("Id").value();
      cout<<"This file '"<<argv[n]<<"' is EML form "<< formid <<endl;
    }
    else {
      cerr<<"This is not EML"<<endl;
      return -1;
    }

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
         510d finally has the results for a gemeeente, plus the individual counting stations as reporting units



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
    */

    if(formid=="110a") { //  verkiezingsdefinitie
      auto start = doc.child("EML").child("ElectionEvent").child("Election");
      // 
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
          for(const auto& n : node) {
            string name=n.name();
            if(name=="kr:RegisteredParty") {
              cout<<"Party: "<<n.child("kr:RegisteredAppellation").begin()->value()<<endl;
            }
          }
        }
        else
          cout << "VERKIEZINGSDEFINITIE: "<<node.name() << endl;
      }
    }
    else if(atoi(formid.c_str())==510) {
      auto start = doc.child("EML").child("Count").child("Election");
      // this contains votes for the current and one level lower:
      // d -> main, c -> kieskring, b -> gemeente (-> a, counting station)
      // TOP     KIESKRING,    GEMEENTE,  STEMBUREAU

      string top("Provincie"), kieskringName, kieskringHSB,  gemeente, stembureau;
      int kieskringId=-1;
      if(formid=="510d") {
        // nothing to set
        cout<<"Top level counts"<<endl;
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
        kieskringName = start.child("Contests").child("Contest").child("ContestIdentifier").child("ContestName").begin()->value();
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
      }

      cout<<"Form "<<formid<<": kieskringName '"<<kieskringName<<"' kieskringHSB '"<< kieskringHSB<<"' kieskringId "<<kieskringId<<endl;
      auto sels = start.child("Contests").child("Contest").child("TotalVotes");
      int affid=-1;
      for(const auto& s : sels) {
        string sname = s.name();

        if(sname=="Selection") {
          int candid=-1;
          string sc;
          bool isAffiliation=false;
          for(const auto& snode : s) {
            string snname = snode.name();
            if(snname=="AffiliationIdentifier") {
              isAffiliation=true;
              if(auto rnnode = snode.child("RegisteredName")) {
                if(rnnode.begin() != rnnode.end())
                  cout<<"Affiliation: "<<snode.child("RegisteredName").begin()->value()<<" (";
              }
              affid=atoi(snode.attribute("Id").value());
              cout<<affid;
              cout<<"): ";
            }
            else if(snname=="Candidate") {
              isAffiliation=false;
              sc = snode.child("CandidateIdentifier").attribute("ShortCode").value();
              string fname;
              
              if(sc.empty()) {
                candid = atoi(snode.child("CandidateIdentifier").attribute("Id").value());
                //                cout<<"Looking up "<<affid<<","<<candid<<endl;
                fname = candnames[{affid,candid}];
              }
              cout<<"Candidate for affid "<<affid<<": "<<sc<<" "<<fname<<" ("<<candid<<"): ";
              
            }
            else if(snname=="ValidVotes") {
              cout << snode.begin()->value() << endl;
              if(!isAffiliation) {
                if(candid<0)
                  sqw.addValue({{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"formid", formid}, {"gemeente", gemeente},
                                {"affid", affid}, {"shortcode", sc}, {"votes", atoi(snode.begin()->value())}}, "candvotecounts");
                else
                  sqw.addValue({{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"formid", formid}, {"gemeente", gemeente},
                                {"affid", affid}, {"candid", candid}, {"votes", atoi(snode.begin()->value())}}, "candvotecounts");
              }
              else {
                  sqw.addValue({{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"formid", formid}, {"gemeente", gemeente},
                                {"affid", affid}, {"votes", atoi(snode.begin()->value())}}, "affvotecounts");
                
              }
            }
            else
              cout<<"Unknown snname "<<snname<<endl;

          }
        }
      }
      for(const auto&s : start.child("Contests").child("Contest")) {
        string name = s.name();
        if(name == "ReportingUnitVotes") {
          int affid=-1;
          string affname;
          string runame, ruId;
          for(const auto& s2 : s) {
            string lname = s2.name();
            if(lname=="ReportingUnitIdentifier") {
              // <ReportingUnitIdentifier Id="HSB1">Kieskring Maastricht</ReportingUnitIdentifier>
              runame = s2.begin()->value();
              ruId = s2.attribute("Id").value();
              cout<<"Counts from ReportingUnit: "<<runame<<endl;
              /* Also has an ID worth storing 1926::SB22, perhaps with the postcode as well */
            }
            else if(lname=="Selection") {
              int validvotes = atoi(s2.child("ValidVotes").begin()->value());
              string stembureau;
              if(formid=="510b") {
                stembureau = runame;
              }
              else if(formid=="510c") {
                gemeente = runame;
              }
              else if(formid == "510d") {
                // <ReportingUnitIdentifier Id="HSB1">Kieskring Maastricht</ReportingUnitIdentifier>
                kieskringName=runame;
                kieskringHSB=ruId;
                kieskringId = atoi(&ruId.at(3));
              }
              
              if(auto cand=s2.child("Candidate")) {
                int candid = atoi(cand.child("CandidateIdentifier").attribute("Id").value());
                cout<<"Candidate "<<candid;
                string shortcode;
                if(auto sc=cand.child("CandidateIdentifier").attribute("ShortCode")) {
                  cout<<" ShortCode "<<sc.value();
                  shortcode = sc.value();
                }
                cout<< " affiliation "<<affid<<": ";
                sqw.addValue({{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"formid", formid}, {"gemeente", gemeente},
                              {"stembureau", stembureau}, {"affid", affid}, {"candid", candid}, {"shortcode", shortcode}, {"votes", validvotes}}, "rucandvotecounts");
              }
              else if(auto aff=s2.child("AffiliationIdentifier")) {
                affid = atoi(aff.attribute("Id").value());
                if(aff.child("RegisteredName").begin() != aff.child("RegisteredName").end())
                  affname = aff.child("RegisteredName").begin()->value();
                else
                  affname="";
                cout<<"Affiliation "<<affid<<" ("<<affname<<"): ";

                sqw.addValue({{"kieskring", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"formid", formid}, {"gemeente", gemeente},
                              {"stembureau", stembureau}, {"affid", affid}, {"votes", validvotes}}, "ruaffvotecounts");

              }

              cout <<validvotes<<endl;
            }
            else
              cout<<s2.name()<<endl;
          }
        }
      }
    }
    else if(formid=="230b") {
      auto start=doc.child("EML").child("CandidateList").child("Election").child("Contest");
      cout<<"Candidate list"<<endl;
      string top("Provincie"), kieskringName, kieskringHSB,  gemeente, stembureau;
      int kieskringId=-1;

      kieskringName = start.child("ContestIdentifier").child("ContestName").begin()->value();
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
          string regname;
          if(auto regnamenode = node.child("AffiliationIdentifier").child("RegisteredName"))
            if(regnamenode.begin() != regnamenode.end())
              regname = regnamenode.begin()->value();
          int id = atoi(node.child("AffiliationIdentifier").attribute("Id").value());
          cout<<"affiliation: "<<regname<< " ("<<id<<")"<<endl;
          sqw.addValue({ {"id", id}, {"kieskringName", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"name", regname}}, "affiliations");
          for(const auto& c : node) {
            string cname = c.name();
            if(cname=="Candidate") {
              cout<<"\t";
              string inits = c.child("CandidateFullName").child("xnl:PersonName").child("xnl:NameLine").begin()->value();
              cout <<inits<<" ";
              auto prefix = c.child("CandidateFullName").child("xnl:PersonName").child("xnl:NamePrefix");
              string prefixpart;
              if(prefix) {
                cout << prefix.begin()->value()<<" ";
                prefixpart = prefix.begin()->value();
              }

              string gender;
              if(auto gennode=c.child("Gender"))
                gender = gennode.begin()->value();

              string firstname;
              if(auto fnnode = c.child("CandidateFullName").child("xnl:PersonName").child("xnl:FirstName"))
                firstname = fnnode.begin()->value();

              string lastname = c.child("CandidateFullName").child("xnl:PersonName").child("xnl:LastName").begin()->value();
              cout <<firstname<<" "<<lastname;
              string woonplaats;
              if(auto wp = c.child("QualifyingAddress").child("xal:Locality").child("xal:LocalityName"))
                woonplaats = wp.begin()->value();
              
              int candid = atoi(c.child("CandidateIdentifier").attribute("Id").value());
              cout << " (" <<candid <<")"<<endl;
              candnames[{id, candid}]  = inits + (prefixpart.empty() ? "" : (" "+prefixpart)) + " " +lastname;
              sqw.addValue({{"kieskringName", kieskringName}, {"kieskringHSB", kieskringHSB}, {"kieskringId", kieskringId}, {"id", candid}, {"affid", id},
                            {"firstname", firstname}, {"initials", inits}, {"prefix", prefixpart}, {"lastname", lastname}, {"gender", gender}, {"woonplaats", woonplaats}
                }, "candentries");
              //              cout<<"Storing {"<<id<<","<<candid<<"}: "<< inits + prefixpart + lastname << endl;
            }
            else if(cname=="AffiliationIdentifier") {
            }
            else if(cname=="Type") {
            }
            else if(cname=="kr:ListData") {
            }
            else
              cout<<"? "<<cname<<endl;
          }
        }
        else cout<<name<<endl;
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

