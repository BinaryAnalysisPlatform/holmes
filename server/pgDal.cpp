#include "pgDal.h"

#include <capnp/message.h>
#include <capnp/pretty-print.h>
#include <kj/debug.h>

#include <iostream>

#include "fact_util.h"

namespace holmes {

void PgDAL::initDB() {
  pqxx::work work(conn);
  work.exec("create schema if not exists facts");
  auto res = work.exec("select table_name, udt_name from information_schema.columns where table_schema = 'facts' ORDER BY table_name, ordinal_position");
  work.commit();
  for (auto line : res) {
    std::string name = line[0].c_str();
    if (types.find(name) == types.end()) {
      std::vector<Holmes::HType> sig;
      types[name] = sig;
    }
    Holmes::HType typ;
    std::string type_string = line[1].c_str();
    if (type_string == "int8") {
      typ = Holmes::HType::ADDR;
    } else if (type_string == "varchar") {
      typ = Holmes::HType::STRING;
    } else if (type_string == "bytea") {
      typ = Holmes::HType::BLOB;
    } else if (type_string == "jsonb") {
      typ = Holmes::HType::JSON;
    } else {
      std::cerr << "Type parse failure: " << type_string << std::endl;
      exit(1);
    }
    types[name].push_back(typ);
  }
  for (auto type : types) {
    registerPrepared(type.first, type.second.size());
  }
}

void PgDAL::registerPrepared(std::string name, size_t n) {
  std::string argVals = "(";
  for (size_t i = 1; i <= n; i++) {
    argVals += "$" + std::to_string(i);
    if (i != n) {
      argVals += ", ";
    } else {
      argVals += ")";
    }
  }
  conn.prepare(name + ".insert", "INSERT INTO facts." + name + " VALUES " + argVals);
}

size_t PgDAL::setFacts(capnp::List<Holmes::Fact>::Reader facts) {
  std::lock_guard<std::mutex> lock(mutex);
  pqxx::work work(conn);
  std::vector<pqxx::result> res;
  for (auto fact : facts) {
    if (!typecheck(types, fact)) {
      LOG(ERROR) << "Bad fact: " << kj::str(capnp::prettyPrint(fact)).cStr();
      throw "Fact Type Error";
    }
    std::string name = fact.getFactName();
    auto query = work.prepared(name + ".insert");
    for (auto arg : fact.getArgs()) {
      switch (arg.which()) {
        case Holmes::Val::JSON_VAL:
          query(std::string(arg.getJsonVal()));
          break;
        case Holmes::Val::STRING_VAL:
          query(std::string(arg.getStringVal()));
          break;
        case Holmes::Val::ADDR_VAL:
        //PgSQL is bad, and only has a signed int type
          query((int64_t)arg.getAddrVal());
          break;
        case Holmes::Val::BLOB_VAL:
          capnp::Data::Reader data = arg.getBlobVal();
          pqxx::binarystring blob(data.begin(), data.size());
          query(blob);
          break;
      }
    }
    res.push_back(query.exec());
  }
  work.commit();
  size_t affected = 0;
  for (auto r : res) {
    affected += r.affected_rows();
  }
  return affected;
}


std::string htype_to_sqltype(Holmes::HType hType) {
  switch (hType) {
    case Holmes::HType::JSON:
      return "jsonb";
    case Holmes::HType::STRING:
      return "varchar";
    case Holmes::HType::ADDR:
      return "bigint";
    case Holmes::HType::BLOB:
      return "bytea";
  }
  return "unknown";
}

bool valid_name(std::string s) {
  for (auto c : s) {
    if (c == '_') {
      continue;
    }
    if ((c >= 'a') && (c <= 'z')) {
      continue;
    }
    if ((c >= '0') && (c <= '9')) {
      continue;
    }
    return false;
  }
  return true;
}

bool PgDAL::addType(std::string name, capnp::List<Holmes::HType>::Reader argTypes) {
  std::lock_guard<std::mutex> lock(mutex);
  //We're using this for a table name, so we have restrictions
  if (!valid_name(name)) {
    return false;
  }
  auto itt = types.find(name);
  if (itt != types.end()) {
    if (argTypes.size() != itt->second.size()) {
      return false;
    }
    for (size_t i = 0; i < argTypes.size(); i++) {
      if (argTypes[i] != itt->second[i]) {
        return false;
      }
    }
    return true;
  } else {
    std::vector<Holmes::HType> sig;
    std::string tableSpec = "(";
    for (size_t i = 0; i < argTypes.size(); i++) {
      tableSpec += "arg" + std::to_string(i) + " " + htype_to_sqltype(argTypes[i]);
      sig.push_back(argTypes[i]);
      if (i == argTypes.size() - 1) {
        tableSpec += ")";
      } else {
        tableSpec += ", ";
      }
    }
    pqxx::work work(conn);
    
    work.exec("CREATE TABLE facts." + name + " " + tableSpec);
    work.commit();
    types[name] = sig;
    registerPrepared(name, sig.size());
    return true;
  }
}

std::string quoteVal(pqxx::work& w, Holmes::Val::Reader v) {
  switch (v.which()) {
    case Holmes::Val::JSON_VAL:
      return w.quote(std::string(v.getJsonVal()));
    case Holmes::Val::STRING_VAL:
      return w.quote(std::string(v.getStringVal()));
    case Holmes::Val::BLOB_VAL:
      //You probably don't want to do this... but for completeness sake
      return w.quote_raw(v.getBlobVal().begin(), v.getBlobVal().size());
    case Holmes::Val::ADDR_VAL:
      //Postgres doesn't support uint64_t
      return w.quote((int64_t)v.getAddrVal());
  }
  throw "Failed to quote value";
}
void buildFromDB(Holmes::HType typ, Holmes::Val::Builder val, pqxx::result::field dbVal) {
  switch (typ) {
    case Holmes::HType::JSON:
      val.setJsonVal(dbVal.as<std::string>());
      break;
    case Holmes::HType::ADDR:
      val.setAddrVal(dbVal.as<int64_t>());
      break;
    case Holmes::HType::STRING:
      val.setStringVal(dbVal.as<std::string>());
      break;
    case Holmes::HType::BLOB:
      pqxx::binarystring bs(dbVal);
      auto bb = val.initBlobVal(bs.size());
      for (size_t k = 0; k < bs.size(); ++k) {
        bb[k] = bs[k];
      }
  }
}
 
std::vector<DAL::Context> PgDAL::getFacts(
  capnp::List<Holmes::FactTemplate>::Reader clauses) {
  std::lock_guard<std::mutex> lock(mutex);
  pqxx::work work(conn);
  std::vector<std::string> whereClause; //Concrete values
  std::vector<std::string> bindName;
  std::vector<Holmes::HType> bindType;
  std::vector<bool> bindAll;
  std::string query = "";
  size_t clauseN = 0;
  for (auto itc = clauses.begin(); itc != clauses.end(); ++itc, ++clauseN) {
    std::string tableName = "facts.";
    tableName += itc->getFactName();
    std::string tableVar = "tbl" + std::to_string(clauseN);
    if (itc == clauses.begin()) {
      query += " FROM ";
    } else if (itc != clauses.end()) {
      query += " JOIN ";
    }
    query += tableName + " " + tableVar;
    if (itc != clauses.begin()) {
      query += " ON ";
    }
    auto args = itc->getArgs();
    bool onClause = true;
    for (size_t i = 0; i < args.size(); ++i) {
      switch (args[i].which()) {
        case Holmes::TemplateVal::EXACT_VAL:
          whereClause.push_back(tableVar + ".arg" + std::to_string(i) + "=" + quoteVal(work, args[i].getExactVal()));
          break;
        case Holmes::TemplateVal::BOUND:
        case Holmes::TemplateVal::FORALL:
          {
            uint32_t var;
            if (args[i].which() == Holmes::TemplateVal::BOUND) {
              var = args[i].getBound();
            } else {
              var = args[i].getForall();
            }
            auto argName = tableVar + ".arg" + std::to_string(i);
            if (var >= bindName.size()) {
            //The variable is mentioned for the first time, this is its
            //cannonical name
              bindName.push_back(argName);
              bindType.push_back(types[itc->getFactName()][i]);
              bindAll.push_back(args[i].which() == Holmes::TemplateVal::FORALL);
            } else {
            //This is a repeat, it needs to be unified
              std::string cond = argName + "=" + bindName[var];
              if (itc == clauses.begin()) {
                //First table has no on clause, stash these in the where clause
                whereClause.push_back(cond);
              } else {
                if (onClause) {
                  onClause = false;
                } else {
                  query += " AND ";
                }
                query += cond + " ";
              }
            }
          }
          break;
        case Holmes::TemplateVal::UNBOUND:
          break;
      }
    }
  }
  for (auto itw = whereClause.begin(); itw != whereClause.end(); ++itw) {
    if (itw == whereClause.begin()) {
      query += " WHERE ";
    } else {
      query += " AND ";
    }
    query += *itw;
  }
  std::string select = "SELECT ";
  std::string groupBy = " GROUP BY ";
  for (size_t i = 0; i < bindName.size(); i++) {
    if (bindAll[i]) {
      select += "array_agg(" + bindName[i] + ")";
    } else {
      select += bindName[i];
      groupBy += bindName[i] + ",";
    }
    if (i + 1 < bindName.size()) {
      select += ", ";
    }
  }
  groupBy.erase(groupBy.size()-1);
  query = select + query + groupBy;
  DLOG(INFO) << "Executing join query: " << query;
  auto res = work.exec(query); 
  work.commit();
  DLOG(INFO) << "Query complete";
  std::vector<Context> ctxs;
  for (auto soln : res) {
    Context ctx;
    for (int i = 0; i < bindType.size(); i++) {
      auto val = ctx.init();
      if (bindAll[i]) {
        //This is an array, and we need to make a list and bind it all.
        //For now, let's return a string just to show it working
        buildFromDB(Holmes::HType::STRING, val, soln[i]);
      } else {
        buildFromDB(bindType[i], val, soln[i]);
      }
    }
    ctxs.push_back(ctx);
  }
  return ctxs;
}

}
