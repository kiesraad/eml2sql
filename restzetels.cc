#include <boost/rational.hpp>
#include "sqlwriter.hh"
#include "nlohmann/json.hpp"
#include <iostream>
#include <mutex>

#include <fmt/format.h>
#include "jsonhelper.hh"
#include "ext/argparse.hpp" 

using namespace std;

template<typename T, typename R>
R genget(const T& cont, const std::string& fname)
{
  R ret{};
  auto iter = cont.find(fname);
  if(iter == cont.end() || !std::get_if<R>(&iter->second))
    return ret;

  return std::get<R>(iter->second);  
}


template<typename T>
std::string eget(const T& cont, const std::string& fname)
{
  return genget<T, std::string>(cont, fname);
}

template<typename T>
int64_t iget(const T& cont, const std::string& fname)
{
  return genget<T, int64_t>(cont, fname);
}


int main()
{
  using boost::rational;
  SQLiteWriter sqw("eml.sqlite");

  auto election = sqw.queryT("select * from election");
  if(election.empty()) {
    cout<<"No election found in database, missing 110a?"<<endl;
    return EXIT_FAILURE;
  }
  if(election.size() != 1) {
    cout<<"Multiple elections in database, don't know which one you want"<<endl;
    return EXIT_FAILURE;
  }

  int numseats= iget(election[0], "seats");


  string electionkind = eget(election[0], "category");
  cout<<"There are "<<numseats<<" seats to allocate in this '"<<electionkind<<"' election"<<endl;  

  bool gemeenteraad = (electionkind=="GR");
  if(gemeenteraad)
    cout<<"This is a gemeenteraad <= 2026 election, no whole seat is required to be elegible for a rest-seat"<<endl;
  
  auto res = sqw.queryT("select affid,name,sum(votes) as votes from affvotecounts,affiliations where affiliations.id=affvotecounts.affid and affiliations.kieskringId = affvotecounts.kieskringId and formid='510b' group by affid");

  //  auto res = sqw.queryT("select affid, \"Lijst \" || affid as name,sum(votes) as votes from affvotecounts where formid='510b' group by affid");

  // affid,partyname,votes
  /*
  std::vector<std::unordered_map<std::string,MiniSQLite::outvar_t>> res =
    {
      {{"affid", 1}, {"name", "Eerlijk alternatief"}, {"votes", 123}},
      {{"affid", 2}, {"name", "D66"}, {"votes", 1230}},
      {{"affid", 3}, {"name", "VVD"}, {"votes", 1223}},
      {{"affid", 4}, {"name", "Progressief PN"}, {"votes", 1223}},
      {{"affid", 5}, {"name", "Trots Pijnacker-Nootdorp"}, {"votes", 1423}},
      {{"affid", 6}, {"name", "CDA"}, {"votes", 1283}},
      {{"affid", 7}, {"name", "Partij voor de Dieren"}, {"votes", 1023}},
      {{"affid", 8}, {"name", "CU SGP"}, {"votes", 1293}},
      {{"affid", 9}, {"name", "Pijnacker-Nootdorp Vooruit (PNV)"}, {"votes", 1293}},
      {{"affid", 10}, {"name", "Politieke Partij voor Basisinkomen"}, {"votes", 1238}},
      {{"affid", 11}, {"name", "Forum voor Democratie"}, {"votes", 1238}},
    };
  */

  int64_t totvotes=0;
  for(auto& row : res)  {
    auto& votes = get<int64_t>(row["votes"]);
    fmt::print("{} (#{}): {}\n",
	       get<string>(row["name"]),
	       get<int64_t>(row["affid"]),
	       votes);
    totvotes+=votes;
  }

;
  rational<int64_t> kiesdelerr(totvotes, numseats);

  auto rformat = [](const rational<int64_t>& in)
  {
    ostringstream ret;
    ret << (in.numerator() / in.denominator());
    auto rest = in  - in.numerator() / in.denominator();
    if(rest > 0)
      ret << " " << rest;
    return ret.str();
  };
  
  fmt::print("\nKiesdeler {}, er waren {} stemmen\n", rformat(kiesdelerr), totvotes); 

  int totzetels = 0;
  struct Toewijzing
  {
    int64_t votes;
    int zetels;
  };
  map<string, Toewijzing> pvotes;  
  for(auto& row : res)  {
    auto votes = get<int64_t>(row["votes"]);
    if(gemeenteraad || votes > kiesdelerr) {
      auto zetels =boost::rational_cast<int>(votes/kiesdelerr);
      auto name = get<string>(row["name"]);
      fmt::print("{}: {} ({} stemmen)\n",
		 name,
		 zetels, votes);
      pvotes[name] = {votes, zetels};
      totzetels += zetels;
    }
  }
  int restzetels = numseats - totzetels;
  fmt::print("{} zetels zijn toegekend, er zijn {} restzetels\n\n",
	     totzetels, restzetels);

  boost::rational<int64_t> bestvpz;
  for(int n = 1; n <= restzetels; ++n) {

    multimap<boost::rational<int64_t>, string> oparties;
    for(const auto& p : pvotes) {
      auto vpz = boost::rational<int64_t>(p.second.votes, p.second.zetels+1); // note the +1
      oparties.insert({-vpz,  p.first});

    }
    for(const auto& p : oparties) {
      fmt::print("{:<45} {} stemmen/zetel\n", p.second, rformat(-p.first));
    }
    fmt::print("Restzetel {} gaat naar {}\n\n", n, oparties.cbegin()->second);
    bestvpz = -oparties.cbegin()->first;
    pvotes[oparties.cbegin()->second].zetels++;
  }

  fmt::print("\nBest vpz: {}\n", rformat(bestvpz));
  totzetels=0;

  struct SorEntry
  {
    int64_t negzetels;
    string name;
    int64_t votes;
    bool operator<(const SorEntry& rhs) const {
      return std::make_tuple(negzetels, name, votes) <
	std::make_tuple(rhs.negzetels, rhs.name, rhs.votes);
    }
  };
  vector<SorEntry> sor;
  for(const auto& p : pvotes) {
    sor.push_back({-p.second.zetels, p.first, p.second.votes});
    totzetels += p.second.zetels;
  }
  sort(sor.begin(), sor.end());

  for(const auto& s : sor) {
    fmt::print("{:<45} {} zetels {} (nodig {})\n", s.name, -s.negzetels,
	       rformat(bestvpz - boost::rational<int64_t>(s.votes, -s.negzetels + 1)) ,
	       rformat(-s.negzetels * (bestvpz - boost::rational<int64_t>(s.votes, -s.negzetels + 1)) ));
  }
  fmt::print("Totaal aantal zetels: {}\n", totzetels);

}
