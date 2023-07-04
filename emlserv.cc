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
                  

int main(int argc, char**argv)
{
  SQLiteWriter sqw("eml.sqlite");
  std::mutex sqwlock;
  // HTTP
  httplib::Server svr;
  svr.set_mount_point("/", "./html/");

  auto result=sqw.query("select * from candentries where affid=?", {4});

  for(auto& r : result) {
    for(const auto& c : r)
      cout<<c.second<<" ";
    cout<<endl;
  }

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

  svr.Get(R"(/elections/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      auto result = lsqw.query("select * from election", {});
      res.set_content(packResults(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  // select gemeenteId,gemeente from affvotecounts where formid='510b' group by 1 order by 1;

  svr.Get(R"(/gemeentes/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      auto result = lsqw.query("select gemeenteId,gemeente from affvotecounts where formid='510b' and electionId=? group by 1 order by 1", {electionId});
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

    //                                     elect gemee  sb
    svr.Get(R"(/stembureau-candvotecount/([^/]*)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      string gemeenteIdstr=req.matches[2];
      string stembureauIdstr=req.matches[3];
      
      int gemeenteId = atoi(gemeenteIdstr.c_str());
      int stembureauId=atoi(stembureauIdstr.c_str());
      auto result = lsqw.query("select * from sbcounts where electionId=? and gemeenteId=? and stembureauId=?", {electionId,gemeenteId,stembureauId});
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

  svr.Get(R"(/totaaltelling/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1];
      auto result = lsqw.query("select affid,votes from affvotecounts where formid='510d' and electionId=?", {electionId});
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

  svr.listen("0.0.0.0", argc==1 ? 8081 : atoi(argv[1]));
}
