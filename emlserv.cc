#include "httplib.h"
#include "sqlwriter.hh"
#include "json.hpp"
#include <iostream>
#include <mutex>

using namespace std;

string packResults(const vector<unordered_map<string,string>>& result)
{
  nlohmann::json arr = nlohmann::json::array();
  
  for(const auto& r : result) {
    nlohmann::json j;
    for(const auto& c : r)
      j[c.first]=c.second;
    arr += j;
  }
  return arr.dump();
}
                  

auto pivot(const vector<unordered_map<string,string>>& result)
{
  nlohmann::json j;
  for(const auto& r : result) {
    map<string,string> kp;
    for(const auto& c : r) {
      kp.insert(c);
    }
    
    string valname;
    string kind = kp["kind"];
    string value = kp["value"];
    j[kind]=atoi(value.c_str());
  }
  return j;
}

int main(int argc, char**argv)
{
  SQLiteWriter sqw("eml.sqlite");
  std::mutex sqwlock;
  // HTTP
  httplib::Server svr;
  svr.set_mount_point("/", "./html/");

  struct LockedSqw
  {
    SQLiteWriter& sqw;
    std::mutex& sqwlock;
    vector<unordered_map<string,string>> query(const std::string& query, const std::initializer_list<SQLiteWriter::var_t>& values)
    {
      std::lock_guard<mutex> l(sqwlock);
      return sqw.query(query, values);
    }
  };
  LockedSqw lsqw{sqw, sqwlock};

  svr.Get(R"(/elections/?)", [&lsqw](const httplib::Request &, httplib::Response &res) {
    try {
      auto result = lsqw.query("select * from election", {});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  svr.Get(R"(/rawsb-meta/([^/]*)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string gemeenteIdstr = req.matches[2];
      string sbIdstr = req.matches[3];
      int gemeenteId = atoi(gemeenteIdstr.c_str());
      int stembureau = atoi(sbIdstr.c_str());

      auto result = lsqw.query("select * from rumeta where gemeenteId = ? and stembureauId = ? and electionId = ?",
                            {gemeenteId, stembureau, electionId});

      nlohmann::json j = pivot(result);
      res.set_content(j.dump(), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  svr.Get(R"(/rawgemeente-meta/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string gemeenteIdstr = req.matches[2];
      int gemeenteId = atoi(gemeenteIdstr.c_str());

      auto result = lsqw.query("select * from meta where gemeenteId = ? and electionId = ?",
                            {gemeenteId, electionId});

      nlohmann::json j = pivot(result);
      res.set_content(j.dump(), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  svr.Get(R"(/raw-meta/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];

      auto result = lsqw.query("select * from meta where formid='510d' and electionId = ?",
                            {electionId});

      res.set_content(pivot(result).dump(), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });



  
  // select gemeenteId,gemeente from affvotecounts where formid='510b' group by 1 order by 1;

  svr.Get(R"(/gemeentes-meta/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      auto result = lsqw.query(
                               R"(select gemeenteId,gemeente, kieskring, 
round(100.0*sum(value) filter (where kind='totalcounted')/sum(value) filter (where kind='totalballots'),1) as opkomst, 
round(100.0*sum(value) filter (where kind='geldige volmachtbewijzen')/sum(value) filter (where kind='totalballots'),1) as volmachtperc,  
round(100.0*sum(value) filter (where kind='blanco')/sum(value) filter (where kind='totalballots'),2) as blancoperc,
round(100.0*sum(value) filter (where kind='ongeldig')/sum(value) filter (where kind='totalballots'),2) as ongeldigperc,
round(100.0*sum(value) filter (where kind='geldige kiezerspassen')/sum(value) filter (where kind='totalballots'),2) as kiespasperc,
sum(value) filter (where kind='toegelaten kiezers') as toegelaten
from meta where formid='510b' and electionId=? group by 1 order by 1)", {electionId});
      
      
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });


  
  svr.Get(R"(/gemeente-meta/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string gemeenteIdstr = req.matches[2];
      int gemeenteId = atoi(gemeenteIdstr.c_str());
      auto result = lsqw.query(
                               R"(select gemeenteId,gemeente, kieskring, 
round(100.0*sum(value) filter (where kind='totalcounted')/sum(value) filter (where kind='totalballots'),1) as opkomst, 
round(100.0*sum(value) filter (where kind='geldige volmachtbewijzen')/sum(value) filter (where kind='totalballots'),1) as volmachtperc,  
round(100.0*sum(value) filter (where kind='blanco')/sum(value) filter (where kind='totalballots'),2) as blancoperc,
round(100.0*sum(value) filter (where kind='ongeldig')/sum(value) filter (where kind='totalballots'),2) as ongeldigperc,
round(100.0*sum(value) filter (where kind='geldige kiezerspassen')/sum(value) filter (where kind='totalballots'),2) as kiespasperc,
sum(value) filter (where kind='toegelaten kiezers') as toegelaten
from meta where formid='510b' and electionId=? and gemeenteId=? group by 1 order by 1)", {electionId, gemeenteId});
      
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });


  // select * from sbmeta where gemeenteId=547;

  svr.Get(R"(/gemeente-sbmeta/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string gemeenteIdstr = req.matches[2];
      int gemeenteId = atoi(gemeenteIdstr.c_str());

      auto result = lsqw.query(
                               R"(select stembureauId, stembureau, postcode, gemeente, gemeenteId,
round(100.0*sum(value) filter (where kind='totalcounted')/sum(value) filter (where kind='totalballots'),1) as opkomst, 
round(100.0*sum(value) filter (where kind='geldige volmachtbewijzen')/sum(value) filter (where kind='totalcounted'),1) as volmachtperc,  
round(100.0*sum(value) filter (where kind='blanco')/sum(value) filter (where kind='totalcounted'),2) as blancoperc,
round(100.0*sum(value) filter (where kind='ongeldig')/sum(value) filter (where kind='totalcounted'),2) as ongeldigperc,
round(100.0*sum(value) filter (where kind='geldige kiezerspassen')/sum(value) filter (where kind='totalcounted'),2) as kiespasperc,
sum(value) filter (where kind='toegelaten kiezers') as toegelaten,
sum(value) filter (where kind='blanco') +  
sum(value) filter (where kind='ongeldig') +
sum(value) filter (where kind='totalcounted') -
sum(value) filter (where kind='toegelaten kiezers') as delta

from rumeta where formid='510b' and electionId=? and gemeenteId=? group by stembureauId order by stembureauId)", {electionId, gemeenteId});

      auto angles=lsqw.query(R"(select stembureauId, 
        degrees(acos(sum(v1.votes*v2.votes)/(sqrt(sum(v1.votes*v1.votes))*sqrt(sum(v2.votes*v2.votes))) )) as degdif
        from 
        ruaffvotecounts v1, 
        (select affid,sum(votes) as votes,gemeenteId from affvotecounts where formid='510b' and electionId=? group by affid,gemeenteId) v2 
        where 
        v1.affid=v2.affid and
        v1.formid='510b' and
        v1.gemeenteId = v2.gemeenteId 
        and v1.electionId = ?
        and v1.gemeenteId = ?
        group by stembureauId 
        having degdif > 0
  order by degdif asc)", {electionId, electionId, gemeenteId});

      map<string, string> degmap;
      for(auto& a: angles)
        degmap[a["stembureauId"]]=a["degdif"];

      for(auto& r : result) {
        r["angle"] = degmap[r["stembureauId"]];
      }
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  
  svr.Get(R"(/meta/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      auto result = lsqw.query(
                               R"(select  
round(100.0*sum(value) filter (where kind='totalcounted')/sum(value) filter (where kind='totalballots'),1) as opkomst, 
round(100.0*sum(value) filter (where kind='geldige volmachtbewijzen')/sum(value) filter (where kind='totalcounted'),1) as volmachtperc,  
round(100.0*sum(value) filter (where kind='blanco')/sum(value) filter (where kind='totalcounted'),2) as blancoperc,
round(100.0*sum(value) filter (where kind='ongeldig')/sum(value) filter (where kind='totalcounted'),2) as ongeldigperc,
round(100.0*sum(value) filter (where kind='geldige kiezerspassen')/sum(value) filter (where kind='totalcounted'),2) as kiespasperc    
from meta where formid='510d' and electionId=?)", {electionId});
      
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  
  
  // 
  
  svr.Get(R"(/stembureaus/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string gemeenteIdstr=req.matches[2];
      auto result = lsqw.query("select * from stembureaus where gemeenteId=? and electionId=?", {gemeenteIdstr, electionId});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  // select * from ruaffvotecounts,affiliations where stembureauId='1980::SB36' and affiliations.id=ruaffvotecounts.affid and affiliations.kieskringId = ruaffvotecounts.kieskringId and affiliations.electionId = ruaffvotecounts.electionId;

  //                                     elect gemee  sb
    svr.Get(R"(/stembureau-affvotecount/([^/]*)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string gemeenteIdstr=req.matches[2];
      string stembureauIdstr=req.matches[3];
      
      int gemeenteId = atoi(gemeenteIdstr.c_str());
      int stembureauId=atoi(stembureauIdstr.c_str());
      auto result = lsqw.query("select * from ruaffvotecounts,affiliations where ruaffvotecounts.electionId=? and gemeenteId=? and stembureauId=? and formid='510b' and ruaffvotecounts.affid=affiliations.id and ruaffvotecounts.kieskringId=affiliations.kieskringId;", {electionId,gemeenteId,stembureauId});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

    //                                     elect gemee  sb    affid
    svr.Get(R"(/stembureau-candvotecount/([^/]*)/(\d+)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string gemeenteIdstr=req.matches[2];
      string stembureauIdstr=req.matches[3];
      string affidStr=req.matches[4];
      
      int gemeenteId = atoi(gemeenteIdstr.c_str());
      int stembureauId=atoi(stembureauIdstr.c_str());
      int affid = atoi(affidStr.c_str());
      auto result = lsqw.query("select *, svotes as votes, affname as name from sbcounts where electionId=? and gemeenteId=? and stembureauId=? and affid=?", {electionId,gemeenteId,stembureauId, affid});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });
    
    svr.Get(R"(/gemeente-affvotecount/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string gemeenteIdstr=req.matches[2];
      
      int gemeenteId = atoi(gemeenteIdstr.c_str());

      auto result = lsqw.query("select * from affvotecounts,affiliations where affvotecounts.electionId=? and gemeenteId=? and formid='510b' and affvotecounts.affid=affiliations.id and affvotecounts.kieskringId=affiliations.kieskringId", {electionId,gemeenteId});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });


    svr.Get(R"(/gemeente-candaffvotecount/([^/]*)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string gemeenteIdstr=req.matches[2];
      string affIdstr=req.matches[3];
      
      int gemeenteId = atoi(gemeenteIdstr.c_str());
      int affId=atoi(affIdstr.c_str());;

      auto result = lsqw.query("select * from candvotecounts,candentries,affiliations where candvotecounts.electionId=? and gemeenteId=? and candvotecounts.affid=? and formid='510b' and candvotecounts.affid=candentries.affid and candvotecounts.kieskringId=candentries.kieskringId and candentries.id = candvotecounts.candid and candvotecounts.electionId = candentries.electionId and affiliations.id=candentries.affid and affiliations.electionId=candvotecounts.electionId and affiliations.kieskringId=candvotecounts.kieskringId", {electionId,gemeenteId, affId});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });



    
  svr.Get(R"(/kieskringen/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      auto result = lsqw.query("select kieskringId,kieskringName,kieskringHSB,electionId from candentries where electionId=? group by 1", {electionId});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  svr.Get(R"(/gemeentes/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      auto result = lsqw.query("select kieskringId,kieskring,kieskringHSB,electionId,gemeente,gemeenteId from meta where gemeente!='' and electionId=? group by gemeenteId", {electionId});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  svr.Get(R"(/affiliations/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      auto result = lsqw.query("select id,kieskringId,kieskringName,electionId,name from affiliations where electionId=?", {electionId});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  svr.Get(R"(/totaaltelling-aff/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      auto result = lsqw.query("select affid, uniaffili.name as name,votes from affvotecounts,uniaffili where uniaffili.id=affvotecounts.affid and formid='510d' and electionId=?", {electionId});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });


  svr.Get(R"(/totaaltelling-affcand/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string affIdstr=req.matches[2];
      int affid= atoi(affIdstr.c_str());
      auto result = lsqw.query("select *,svotes as votes from escounts where electionId=? and affid=?", {electionId, affid});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  
  svr.Get(R"(/totaaltellingen/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      struct Votes
      {
        unsigned int f510d, f510c, f510b, f510a;
        bool discrepancy{false};
      };
      map<int,Votes> votes;
      
      auto result = lsqw.query("select affid,sum(votes) as svotes from affvotecounts where formid='510d' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[atoi(r["affid"].c_str())].f510d = atoi(r["svotes"].c_str());
      }

      result = lsqw.query("select affid,sum(votes) as svotes from affvotecounts where formid='510c' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[atoi(r["affid"].c_str())].f510c = atoi(r["svotes"].c_str());
      }

      result = lsqw.query("select affid,sum(votes) as svotes from affvotecounts where formid='510b' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[atoi(r["affid"].c_str())].f510b = atoi(r["svotes"].c_str());
      }

      result = lsqw.query("select affid,sum(votes) as svotes from ruaffvotecounts where formid='510b' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[atoi(r["affid"].c_str())].f510a = atoi(r["svotes"].c_str());
      }
      vector<unordered_map<string,string>> endres;
      for(auto& v : votes) {
        
        if(v.second.f510d != v.second.f510c || v.second.f510c != v.second.f510b || v.second.f510b != v.second.f510a) {
          v.second.discrepancy = true;
        }
        unordered_map<string,string> row;
        row["affid"]=to_string(v.first);
        row["votes510d"] = to_string(v.second.f510d);
        row["votes510c"] = to_string(v.second.f510c);
        row["votes510b"] = to_string(v.second.f510b);
        row["votes510a"] = to_string(v.second.f510a);
        row["discrepancy"] = v.second.discrepancy ? "true" : "false";
        endres.push_back(row);
      }
      
      res.set_content(packResults(endres), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });


 svr.Get(R"(/totaaltellingen-per-kieskring/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      struct Votes
      {
        unsigned int f510d, f510c, f510b, f510a;
        bool discrepancy{false};
      };
      map<int,Votes> votes;
      
      auto result = lsqw.query("select kieskringId,sum(votes) as svotes from ruaffvotecounts where formid='510d' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[atoi(r["kieskringId"].c_str())].f510d = atoi(r["svotes"].c_str());
      }

      result = lsqw.query("select kieskringId,sum(votes) as svotes from affvotecounts where formid='510c' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[atoi(r["kieskringId"].c_str())].f510c = atoi(r["svotes"].c_str());
      }

      result = lsqw.query("select kieskringId,sum(votes) as svotes from affvotecounts where formid='510b' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[atoi(r["kieskringId"].c_str())].f510b = atoi(r["svotes"].c_str());
      }

      result = lsqw.query("select kieskringId,sum(votes) as svotes from ruaffvotecounts where formid='510b' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[atoi(r["kieskringId"].c_str())].f510a = atoi(r["svotes"].c_str());
      }
      vector<unordered_map<string,string>> endres;
      for(auto& v : votes) {
        
        if(v.second.f510d != v.second.f510c || v.second.f510c != v.second.f510b || v.second.f510b != v.second.f510a) {
          v.second.discrepancy = true;
        }
        unordered_map<string,string> row;
        row["kieskringId"]=to_string(v.first);
        row["votes510d"] = to_string(v.second.f510d);
        row["votes510c"] = to_string(v.second.f510c);
        row["votes510b"] = to_string(v.second.f510b);
        row["votes510a"] = to_string(v.second.f510a);
        row["discrepancy"] = v.second.discrepancy ? "true" : "false";
        endres.push_back(row);
      }
      
      res.set_content(packResults(endres), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

 
  
  //select kieskringId,sum(votes) from ruaffvotecounts where formid='510d' and electionId=? group by kieskringId 
  svr.Get(R"(/totaaltelling-per-kieskring/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      auto result = lsqw.query("select kieskringId,sum(votes) as svotes from ruaffvotecounts where formid='510d' and electionId=? group by kieskringId", {electionId});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });
  
  svr.Get(R"(/gemeentes/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      int kieskringid = stoi(req.matches[2].str());
      auto result = lsqw.query("select kieskringId,kieskring,kieskringHSB,electionId,gemeente,gemeenteId from meta where gemeente!='' and electionId=? and kieskringId=? group by gemeenteId", {electionId, kieskringid});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  
  svr.Get(R"(/candentries/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string election = req.matches[1];
      int kieskringid = stoi(req.matches[2].str());
      auto result = lsqw.query("select * from candentries where electionID=? and kieskringId=?", {election,kieskringid});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  svr.Get(R"(/candentries/([^/]*)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string election = req.matches[1];
      int kieskringid = stoi(req.matches[2].str());
      int affid = stoi(req.matches[3].str());
      auto result = lsqw.query("select * from candentries where electionID=? and kieskringId=? and affid=?", {election,kieskringid,affid});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  svr.Get(R"(/candentries/([^/]*)/(\d+)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string election = req.matches[1];
      int kieskringid = stoi(req.matches[2].str());
      int affid = stoi(req.matches[3].str());
      int candid = stoi(req.matches[4].str());
      auto result = lsqw.query("select * from candentries where electionID=? and kieskringId=? and affid=? and id=?",
                               {election,kieskringid,affid,candid});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  int port = argc==1 ? 8081 : atoi(argv[1]);
  cout<<"Binding to port "<<port<<endl;
  svr.listen("0.0.0.0", port);
}
