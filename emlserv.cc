#include "httplib.h"
#include "sqlwriter.hh"
#include "nlohmann/json.hpp"
#include <iostream>
#include <mutex>
#include "jsonhelper.hh"
#include "ext/argparse.hpp" 

using namespace std;

auto pivot(const vector<unordered_map<string, MiniSQLite::outvar_t>>& result)
{
  nlohmann::json j;
  for(const auto& r : result) {
    map<string, MiniSQLite::outvar_t> kp;
    for(const auto& c : r) {
      kp.insert(c);
    }
    
    string valname;
    string kind = get<string>(kp["kind"]);
    int64_t value = get<int64_t>(kp["value"]);
    j[kind]=value;
  }
  return j;
}

int main(int argc, char**argv)
{
  argparse::ArgumentParser program("tensor-convo-par");

  program.add_argument("db-dir").help("directory to read database from").default_value("./");
  program.add_argument("-p", "--port").help("port number to listen on").default_value(8082).scan<'i', int>();
  
  try {
    program.parse_args(argc, argv);
  }
  catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }
  
  cout<<"db-dir: "<<program.get<string>("db-dir") << endl;
  
  SQLiteWriter sqw(program.get<string>("db-dir")+"/eml.sqlite");
  std::mutex sqwlock;
  httplib::Server svr;
  svr.set_mount_point("/", "./html/");

  struct LockedSqw
  {
    SQLiteWriter& sqw;
    std::mutex& sqwlock;
    vector<unordered_map<string, MiniSQLite::outvar_t>> query(const std::string& query, const std::initializer_list<SQLiteWriter::var_t>& values)
    {
      std::lock_guard<mutex> l(sqwlock);
      return sqw.queryT(query, values);
    }

    void queryJ(httplib::Response &res, const std::string& q, const std::initializer_list<SQLiteWriter::var_t>& values) 
    try
    {
      auto result = query(q, values);
      res.set_content(packResultsJsonStr(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  };
  LockedSqw lsqw{sqw, sqwlock};

  svr.Get(R"(/elections/?)", [&lsqw](const httplib::Request &, httplib::Response &res) {
    lsqw.queryJ(res, "select * from election", {});
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
    string electionId=req.matches[1];
    lsqw.queryJ(res,
                               R"(select gemeenteId,gemeente, kieskring, 
round(100.0*sum(value) filter (where kind='totalcounted')/sum(value) filter (where kind='totalballots'),1) as opkomst, 
round(100.0*sum(value) filter (where kind='geldige volmachtbewijzen')/sum(value) filter (where kind='totalballots'),1) as volmachtperc,  
round(100.0*sum(value) filter (where kind='blanco')/sum(value) filter (where kind='totalballots'),2) as blancoperc,
round(100.0*sum(value) filter (where kind='ongeldig')/sum(value) filter (where kind='totalballots'),2) as ongeldigperc,
round(100.0*sum(value) filter (where kind='geldige kiezerspassen')/sum(value) filter (where kind='totalballots'),2) as kiespasperc,
sum(value) filter (where kind='toegelaten kiezers') as toegelaten
from meta where formid='510b' and electionId=? group by 1 order by 1)", {electionId});
  });


  
  svr.Get(R"(/gemeente-meta/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    string gemeenteIdstr = req.matches[2];
    int gemeenteId = atoi(gemeenteIdstr.c_str());
    lsqw.queryJ(res,
                               R"(select gemeenteId,gemeente, kieskring, 
round(100.0*sum(value) filter (where kind='totalcounted')/sum(value) filter (where kind='totalballots'),1) as opkomst, 
round(100.0*sum(value) filter (where kind='geldige volmachtbewijzen')/sum(value) filter (where kind='totalballots'),1) as volmachtperc,  
round(100.0*sum(value) filter (where kind='blanco')/sum(value) filter (where kind='totalballots'),2) as blancoperc,
round(100.0*sum(value) filter (where kind='ongeldig')/sum(value) filter (where kind='totalballots'),2) as ongeldigperc,
round(100.0*sum(value) filter (where kind='geldige kiezerspassen')/sum(value) filter (where kind='totalballots'),2) as kiespasperc,
sum(value) filter (where kind='toegelaten kiezers') as toegelaten
from meta where formid='510b' and electionId=? and gemeenteId=? group by 1 order by 1)", {electionId, gemeenteId});
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
sum(value) filter (where kind='geen verklaring') as geenverklaring,
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

      map<int, double> degmap;
      for(auto& a: angles)
        degmap[get<int64_t>(a["stembureauId"])]=get<double>(a["degdif"]);

      for(auto& r : result) {
        r["angle"] = degmap[get<int64_t>(r["stembureauId"])];
      }
      res.set_content(packResultsJsonStr(result), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

  
  svr.Get(R"(/meta/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    lsqw.queryJ(res,
                               R"(select  
round(100.0*sum(value) filter (where kind='totalcounted')/sum(value) filter (where kind='totalballots'),1) as opkomst, 
round(100.0*sum(value) filter (where kind='geldige volmachtbewijzen')/sum(value) filter (where kind='totalcounted'),1) as volmachtperc,  
round(100.0*sum(value) filter (where kind='blanco')/sum(value) filter (where kind='totalcounted'),2) as blancoperc,
round(100.0*sum(value) filter (where kind='ongeldig')/sum(value) filter (where kind='totalcounted'),2) as ongeldigperc,
round(100.0*sum(value) filter (where kind='geldige kiezerspassen')/sum(value) filter (where kind='totalcounted'),2) as kiespasperc    
from meta where formid='510d' and electionId=?)", {electionId});
  });
  
  svr.Get(R"(/stembureaus/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    string gemeenteIdstr=req.matches[2];
    lsqw.queryJ(res, "select * from stembureaus where gemeenteId=? and electionId=?", {gemeenteIdstr, electionId});
  });

  
  //                                     elect gemee  sb
  svr.Get(R"(/stembureau-affvotecount/([^/]*)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    string gemeenteIdstr=req.matches[2];
    string stembureauIdstr=req.matches[3];
    
    int gemeenteId = atoi(gemeenteIdstr.c_str());
    int stembureauId=atoi(stembureauIdstr.c_str());
    lsqw.queryJ(res, "select * from ruaffvotecounts,affiliations where ruaffvotecounts.electionId=? and gemeenteId=? and stembureauId=? and formid='510b' and ruaffvotecounts.affid=affiliations.id and ruaffvotecounts.kieskringId=affiliations.kieskringId;", {electionId,gemeenteId,stembureauId});
  });

  //  expand this one to get all the stembureaus (voting stations) in one municipality
  svr.Get(R"(/candidate-municipalities/([^/]*)/(\d+)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    try {
      string electionId=req.matches[1]; 
      string kieskringId=req.matches[2]; // this is only used to select the candidate
      string affid=req.matches[3];
      string candid=req.matches[4];

      auto result =lsqw.query("select * from candentries where electionId=? and kieskringId=? and affid=? and id=?",
                              {electionId, kieskringId, affid, candid});

      if(!result.empty()) {
        auto res2=lsqw.query(R"(select votes,cc.gemeente,cc.gemeenteId, value as totvotes, round(100.0*votes/value,3) as perc from candvotecounts cc, candentries ce, meta where cc.formid='510b' and cc.formid=meta.formid and cc.kieskringId=ce.kieskringId and cc.electionId = ce.electionId and ce.electionId = meta.electionId and cc.gemeenteId = meta.gemeenteId and kind='totalcounted' and cc.affid = ce.affid and cc.candid=ce.id and ce.electionId=? and lastname=? and firstname=? and cc.affid=? order by perc desc)", {electionId, get<string>(result[0]["lastname"]), get<string>(result[0]["firstname"]), affid});
        
          auto rows = packResultsJson(res2);
          nlohmann::json ret;
          for(const auto& s : {"lastname", "firstname", "prefix", "gender", "woonplaats", "initials"})
            ret[s]=get<string>(result[0][s]);
          ret["rows"]=rows;
          res.set_content(ret.dump(), "application/json");
        }
        else {
          cout<<"No hits "<<endl;
        }
      }
      catch(exception& e) {
        cerr<<"Error: "<<e.what()<<endl;
      }
        
    });

  //                                     elect gemee  sb    affid
  svr.Get(R"(/stembureau-candvotecount/([^/]*)/(\d+)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
      string electionId=req.matches[1];
      string gemeenteIdstr=req.matches[2];
      string stembureauIdstr=req.matches[3];
      string affidStr=req.matches[4];
      
      int gemeenteId = atoi(gemeenteIdstr.c_str());
      int stembureauId=atoi(stembureauIdstr.c_str());
      int affid = atoi(affidStr.c_str());
      lsqw.queryJ(res, "select *, svotes as votes, affname as name from sbcounts where electionId=? and gemeenteId=? and stembureauId=? and affid=?", {electionId,gemeenteId,stembureauId, affid});
  });
  
  svr.Get(R"(/gemeente-affvotecount/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    string gemeenteIdstr=req.matches[2];
    
    int gemeenteId = atoi(gemeenteIdstr.c_str());
    
    lsqw.queryJ(res, "select * from affvotecounts,affiliations where affvotecounts.electionId=? and gemeenteId=? and formid='510b' and affvotecounts.affid=affiliations.id and affvotecounts.kieskringId=affiliations.kieskringId", {electionId,gemeenteId});
  });
  
  
  svr.Get(R"(/gemeente-candaffvotecount/([^/]*)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    string gemeenteIdstr=req.matches[2];
    string affIdstr=req.matches[3];
    
    int gemeenteId = atoi(gemeenteIdstr.c_str());
    int affId=atoi(affIdstr.c_str());;
    
    lsqw.queryJ(res, "select * from candvotecounts,candentries,affiliations where candvotecounts.electionId=? and gemeenteId=? and candvotecounts.affid=? and formid='510b' and candvotecounts.affid=candentries.affid and candvotecounts.kieskringId=candentries.kieskringId and candentries.id = candvotecounts.candid and candvotecounts.electionId = candentries.electionId and affiliations.id=candentries.affid and affiliations.electionId=candvotecounts.electionId and affiliations.kieskringId=candvotecounts.kieskringId", {electionId,gemeenteId, affId});
  });
  
  svr.Get(R"(/kieskringen/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    lsqw.queryJ(res, "select kieskringId,kieskringName,kieskringHSB,electionId from candentries where electionId=? group by 1", {electionId});
  });
  
  svr.Get(R"(/gemeentes/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    lsqw.queryJ(res,"select kieskringId,kieskring,kieskringHSB,electionId,gemeente,gemeenteId from meta where gemeente!='' and electionId=? group by gemeenteId", {electionId});
  });

  svr.Get(R"(/affiliations/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    lsqw.queryJ(res, "select id,kieskringId,kieskringName,electionId,name from affiliations where electionId=?", {electionId});
  });

  svr.Get(R"(/candidates/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1]; // we ignore this now
    lsqw.queryJ(res, R"(select sum(votes) as svotes,round(avg(candid),2) as avgpos,name,initials,firstname,prefix,lastname,woonplaats,gender,candentries.affid,min(printf("[%d,%d,%d]",candentries.kieskringId,candentries.affid,candentries.id)) as candkey from candvotecounts,candentries,uniaffili where candvotecounts.affid=candentries.affid and candvotecounts.kieskringId = candentries.kieskringId and candentries.id = candvotecounts.candid and uniaffili.id=candentries.affid and formid='510b' group by 3,4,5,6,7,8,9,10 order by 1 desc)", {});
  });

    
  svr.Get(R"(/totaaltelling-aff/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    lsqw.queryJ(res, "select affid, uniaffili.name as name,votes from affvotecounts,uniaffili where uniaffili.id=affvotecounts.affid and formid='510d' and electionId=?", {electionId});
  });
  
  
  svr.Get(R"(/totaaltelling-affcand/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    string affIdstr=req.matches[2];
    int affid= atoi(affIdstr.c_str());
    lsqw.queryJ(res, "select *,svotes as votes from escounts where electionId=? and affid=?", {electionId, affid});
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
        votes[get<int64_t>(r["affid"])].f510d = get<int64_t>(r["svotes"]);
      }

      result = lsqw.query("select affid,sum(votes) as svotes from affvotecounts where formid='510c' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[get<int64_t>(r["affid"])].f510c = get<int64_t>(r["svotes"]);
      }

      result = lsqw.query("select affid,sum(votes) as svotes from affvotecounts where formid='510b' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[get<int64_t>(r["affid"])].f510b = get<int64_t>(r["svotes"]);
      }

      result = lsqw.query("select affid,sum(votes) as svotes from ruaffvotecounts where formid='510b' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[get<int64_t>(r["affid"])].f510a = get<int64_t>(r["svotes"]);
      }
      vector<unordered_map<string, MiniSQLite::outvar_t>> endres;
      for(auto& v : votes) {
        
        if(v.second.f510d != v.second.f510c || v.second.f510c != v.second.f510b || v.second.f510b != v.second.f510a) {
          v.second.discrepancy = true;
        }
        unordered_map<string, MiniSQLite::outvar_t> row;
        row["affid"]=v.first;
        row["votes510d"] = v.second.f510d;
        row["votes510c"] = v.second.f510c;
        row["votes510b"] = v.second.f510b;
        row["votes510a"] = v.second.f510a;
        row["discrepancy"] = v.second.discrepancy ? "true" : "false";
        endres.push_back(row);
      }
      
      res.set_content(packResultsJsonStr(endres), "application/json");
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
        votes[get<int64_t>(r["kieskringId"])].f510d = get<int64_t>(r["svotes"]);
      }

      result = lsqw.query("select kieskringId,sum(votes) as svotes from affvotecounts where formid='510c' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[get<int64_t>(r["kieskringId"])].f510c = get<int64_t>(r["svotes"]);
      }

      result = lsqw.query("select kieskringId,sum(votes) as svotes from affvotecounts where formid='510b' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[get<int64_t>(r["kieskringId"])].f510b = get<int64_t>(r["svotes"]);
      }

      result = lsqw.query("select kieskringId,sum(votes) as svotes from ruaffvotecounts where formid='510b' and electionId=? group by 1 order by 1 ", {electionId});
      for(auto& r : result) {
        votes[get<int64_t>(r["kieskringId"])].f510a = get<int64_t>(r["svotes"]);
      }
      vector<unordered_map<string,MiniSQLite::outvar_t>> endres;
      for(auto& v : votes) {
        
        if(v.second.f510d != v.second.f510c || v.second.f510c != v.second.f510b || v.second.f510b != v.second.f510a) {
          v.second.discrepancy = true;
        }
        unordered_map<string, MiniSQLite::outvar_t> row;
        row["kieskringId"]= v.first;
        row["votes510d"] = v.second.f510d;
        row["votes510c"] = v.second.f510c;
        row["votes510b"] = v.second.f510b;
        row["votes510a"] = v.second.f510a;
        row["discrepancy"] = v.second.discrepancy ? "true" : "false";
        endres.push_back(row);
      }
      
      res.set_content(packResultsJsonStr(endres), "application/json");
    }
    catch(exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
    }
  });

 
  
  //select kieskringId,sum(votes) from ruaffvotecounts where formid='510d' and electionId=? group by kieskringId 
  svr.Get(R"(/totaaltelling-per-kieskring/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    lsqw.queryJ(res, "select kieskringId,sum(votes) as svotes from ruaffvotecounts where formid='510d' and electionId=? group by kieskringId", {electionId});
  });
  
  svr.Get(R"(/gemeentes/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string electionId=req.matches[1];
    int kieskringid = stoi(req.matches[2].str());
    lsqw.queryJ(res, "select kieskringId,kieskring,kieskringHSB,electionId,gemeente,gemeenteId from meta where gemeente!='' and electionId=? and kieskringId=? group by gemeenteId", {electionId, kieskringid});
  });

  
  svr.Get(R"(/candentries/([^/]*)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string election = req.matches[1];
    int kieskringid = stoi(req.matches[2].str());
    lsqw.queryJ(res, "select * from candentries where electionID=? and kieskringId=?", {election,kieskringid});
  });

  svr.Get(R"(/candentries/([^/]*)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string election = req.matches[1];
    int kieskringid = stoi(req.matches[2].str());
    int affid = stoi(req.matches[3].str());
    lsqw.queryJ(res, "select * from candentries where electionID=? and kieskringId=? and affid=?", {election,kieskringid,affid});
  });

  svr.Get(R"(/candentries/([^/]*)/(\d+)/(\d+)/(\d+)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string election = req.matches[1];
    int kieskringid = stoi(req.matches[2].str());
    int affid = stoi(req.matches[3].str());
    int candid = stoi(req.matches[4].str());
    lsqw.queryJ(res,"select * from candentries where electionID=? and kieskringId=? and affid=? and id=?",
                             {election,kieskringid,affid,candid});
  });

  svr.Get(R"(/onverklaard-stembureaus/([^/]*)/?)", [&lsqw](const httplib::Request &req, httplib::Response &res) {
    string election = req.matches[1];
    lsqw.queryJ(res, R"(select gemeente,stembureauId,stembureau,gemeenteId,round(100.0*sum(value) filter (where kind="geen verklaring")/sum(value) filter (where kind='totalcounted'),2) as percgeenverklaring, sum(value) filter (where kind="geen verklaring") as absgeenverklaring, sum(value) filter (where kind="minder getelde stembiljetten") as mindergeteld, sum(value) filter (where kind="meer getelde stembiljetten") as meergeteld, sum(value) filter (where kind="toegelaten kiezers") as toegelaten, sum(value) filter (where kind="meegenomen stembiljetten") as meegenomen, sum(value) filter (where kind="andere verklaring") as andere, sum(value) filter (where kind="te weinig uitgereikte stembiljetten") as teweinig, sum(value) filter (where kind="te veel uitgereikte stembiljetten") as teveel from rumeta where formid='510b' and electionId=? group by 1,2,3 order by absgeenverklaring desc limit 1000)", {election});
  });
    
  int port = program.get<int>("port");
  cout<<"Binding to 0.0.0.0:"<<port<<", try http://127.0.0.1:"<<port<<endl;
  svr.listen("0.0.0.0", port);
}
