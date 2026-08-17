#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <string>
#include "httplib.h"

using namespace std;
using namespace httplib;

struct Problem { int id; string title; string topic; int difficulty; string url; };
struct HistoryItem { string title; string time_taken; };

vector<Problem> recs;
vector<HistoryItem> historyList;
int totalSolved = 0;
sqlite3* DB; 

string cleanString(const string& s) {
    string out = "";
    for (char c : s) {
        if (c == '"') out += "\\\"";                     // Escape quotes correctly
        else if (c == '\\') out += "\\\\";               // Escape backslashes
        else if (c == '\n' || c == '\r' || c == '\t') out += " "; // Turn tabs and newlines into safe spaces
        else if (c >= 0 && c <= 31) continue;            // Delete any other invisible ASCII control characters
        else out += c;
    }
    return out;
}

// SQL Callbacks with strict NULL-checking
static int recCallback(void* data, int argc, char** argv, char** azColName) {
    Problem p;
    p.id = argv[0] ? stoi(argv[0]) : 0; 
    p.title = argv[1] ? argv[1] : "Unknown"; 
    p.topic = argv[2] ? argv[2] : "Unknown"; 
    p.difficulty = argv[3] ? stoi(argv[3]) : 0; 
    p.url = argv[4] ? argv[4] : "#";
    recs.push_back(p); 
    return 0; 
}

static int histCallback(void* data, int argc, char** argv, char** azColName) {
    HistoryItem h; 
    h.title = argv[0] ? argv[0] : "Unknown"; 
    h.time_taken = argv[1] ? argv[1] : "0";
    historyList.push_back(h); 
    return 0; 
}

static int statCallback(void* data, int argc, char** argv, char** azColName) {
    totalSolved = argv[0] ? stoi(argv[0]) : 0; 
    return 0; 
}

int main() {
    sqlite3_open("db/tracker.db", &DB);
    Server svr;
    svr.set_mount_point("/", "./public");

    svr.Get("/api/recommendations", [](const Request& req, Response& res) {
        recs.clear(); 
        string sql = "SELECT pb.problem_id, pb.title, pb.topic, pb.difficulty_rating, pb.leetcode_url "
                     "FROM Problem_Bank pb JOIN Topic_Ratings tr ON pb.topic = tr.topic "
                     "WHERE pb.difficulty_rating >= tr.current_rating "
                     "AND pb.problem_id NOT IN (SELECT problem_id FROM User_History) "
                     "ORDER BY pb.difficulty_rating ASC LIMIT 3;";
        sqlite3_exec(DB, sql.c_str(), recCallback, NULL, NULL);

        string json = "[";
        for (size_t i = 0; i < recs.size(); ++i) {
            json += "{\"id\": " + to_string(recs[i].id) + ", \"title\": \"" + cleanString(recs[i].title) + "\", "
                 + "\"topic\": \"" + cleanString(recs[i].topic) + "\", \"difficulty\": " + to_string(recs[i].difficulty) + ", "
                 + "\"url\": \"" + cleanString(recs[i].url) + "\"}";
            if (i < recs.size() - 1) json += ",";
        }
        json += "]";
        res.set_content(json, "application/json");
    });

    svr.Get("/api/history", [](const Request& req, Response& res) {
        historyList.clear();
        totalSolved = 0;
        
        sqlite3_exec(DB, "SELECT COUNT(DISTINCT problem_id) FROM User_History;", statCallback, NULL, NULL);
        
        string sqlHist = "SELECT pb.title, uh.time_taken_minutes FROM User_History uh "
                         "JOIN Problem_Bank pb ON uh.problem_id = pb.problem_id "
                         "ORDER BY uh.rowid DESC LIMIT 5;";
        sqlite3_exec(DB, sqlHist.c_str(), histCallback, NULL, NULL);

        string json = "{\"total_solved\": " + to_string(totalSolved) + ", \"history\": [";
        for (size_t i = 0; i < historyList.size(); ++i) {
            json += "{\"title\": \"" + cleanString(historyList[i].title) + "\", \"time\": \"" + cleanString(historyList[i].time_taken) + "\"}";
            if (i < historyList.size() - 1) json += ",";
        }
        json += "]}";
        res.set_content(json, "application/json");
    });

    svr.Post("/api/add_problem", [](const Request& req, Response& res) {
        string title = req.get_param_value("title");
        string topic = req.get_param_value("topic");
        string diff = req.get_param_value("difficulty");
        string url = req.get_param_value("url");

        string topicSql = "INSERT OR IGNORE INTO Topic_Ratings (topic, current_rating) VALUES ('" + topic + "', 0);";
        sqlite3_exec(DB, topicSql.c_str(), NULL, NULL, NULL);

        string sql = "INSERT INTO Problem_Bank (title, topic, difficulty_rating, leetcode_url) "
                     "VALUES ('" + title + "', '" + topic + "', " + diff + ", '" + url + "');";
        
        char* messageError = nullptr;
        int exit = sqlite3_exec(DB, sql.c_str(), NULL, NULL, &messageError);
        
        if (exit == SQLITE_OK) {
            res.set_content("Successfully added to Problem Bank!", "text/plain");
        } else {
            string errMsg = "Database Error: ";
            if(messageError) { errMsg += messageError; sqlite3_free(messageError); }
            res.set_content(errMsg, "text/plain");
        }
    });

    svr.Post("/api/solve", [](const Request& req, Response& res) {
        string probId = req.get_param_value("problemId");
        string time = req.get_param_value("timeTaken");
        string diff = req.get_param_value("difficulty");
        string topic = req.get_param_value("topic");

        string histSql = "INSERT INTO User_History (problem_id, date_solved, time_taken_minutes, solved_optimally) "
                         "VALUES (" + probId + ", date('now'), " + time + ", 1);";
        sqlite3_exec(DB, histSql.c_str(), NULL, NULL, NULL);

        string ratingSql = "UPDATE Topic_Ratings SET current_rating = " + diff + " WHERE topic = '" + topic + "';";
        sqlite3_exec(DB, ratingSql.c_str(), NULL, NULL, NULL);

        res.set_content("History logged and Topic Rating updated!", "text/plain");
    });

    cout << "API Server running at http://localhost:8080" << endl;
    svr.listen("localhost", 8080);
    sqlite3_close(DB);
    return 0;
}