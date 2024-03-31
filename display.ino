#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// THESE ARE THE VALUES THAT MUST BE CHANGED

// WiFi credentials
char ssid[] = "";
char password[] = "";

// Index of the sport (Check README for your sport's number)
int apiIndex = 0;

// Index of the team (Check README for your team's number)
int teamIndex = 0;

// How often you want the scores to update in minutes
const long minutes = 5;

////////////////////////////////////////////

WiFiClientSecure client;

String nhlTeams[] = {"ANA", "ARI", "BOS", "BUF", "CGY", "CAR", "CHI", "COL", "CBJ", "DAL", "DET", "EDM", "FLA", "LAK", "MIN", "MTL", "NSH", "NJD", "NYI", "NYR", "OTT", "PHI", "PIT", "SJS", "SEA", "STL", "TBL", "TOR", "VAN", "VGK", "WSH", "WPG"};
int teamIds[4][32] = {
  {57, 58, 402, 397, 328, 61, 354, 62, 341, 338, 64, 65, 66, 67, 68, 340, 73, 346, 563, 76, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 158, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32}
};

const char* baseUrls[] = {"api.football-data.org", "statsapi.mlb.com", "api-web.nhle.com", "tank01-nfl-live-in-game-real-time-statistics-nfl.p.rapidapi.com"};
const char* apiKeys[] = {"9b058c7bb14b4b48bde465ad8783823b", "none", "none", "8397916b8dmshc4fe775ebddd378p1c43ffjsn577a3cae20a4"};
const char* firstHalfQueries[] = {"/v4/teams/", "/api/v1/schedule/games/?sportId=1&teamId=", "/v1/scoreboard/", "/getNFLTeamSchedule?teamID="};
const char* secondHalfQueries[] = {"/matches?status=FINISHED,IN_PLAY&limit=1", "", "/now", ""};
const char* apiNames[] = {"Premier League", "MLB", "NHL", "NFL"};

const int i2c_address = 0x27;
const int lcd_columns = 16;
const int lcd_rows = 2;
LiquidCrystal_I2C lcd(i2c_address, lcd_columns, lcd_rows);

const long interval = 60000 * minutes;

void setup() {
  client.setInsecure();

  lcd.init();
  lcd.backlight();
  lcd.clear();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  makeHTTPRequest();
}

void loop() {
  static unsigned long previousMillis = 0;

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    makeHTTPRequest();
    previousMillis = currentMillis;
  }
}

void printScores(String homeName, String awayName, int homeScore, int awayScore) {
  lcd.setCursor(0,0);
  lcd.print(homeName);
  lcd.print(": ");
  lcd.print(homeScore);
  lcd.setCursor(0, 1);
  lcd.print(awayName);
  lcd.print(": ");
  lcd.print(awayScore);
}

int getEffectiveLength(int array[], int maxSize) {
  for (int i = 0; i < maxSize; i++) {
    if (array[i] == 0) {
      return i; 
    }
  }
  return maxSize;
}

void makeHTTPRequest() {
  if (!client.connect(baseUrls[apiIndex], 443)) {
    Serial.println(F("Connection failed"));
    return;
  }

  client.print(F("GET "));
  client.print(firstHalfQueries[apiIndex]);
  if(apiIndex == 2) {
    client.print(nhlTeams[teamIndex % 30]);
  }
  else {
    client.print(teamIds[apiIndex][teamIndex % getEffectiveLength(teamIds[apiIndex], 32)]);
  }
  client.print(secondHalfQueries[apiIndex]);
  client.println(F(" HTTP/1.1"));

  client.print(F("Host: "));
  client.println(baseUrls[apiIndex]);

  client.print(F("Authorization: "));
  client.println(apiKeys[apiIndex]);

  client.print(F("X-Auth-Token: "));
  client.println(apiKeys[apiIndex]);

  client.print(F("X-RapidAPI-Key: "));
  client.println(apiKeys[apiIndex]);

  if (client.println() == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Connection");
    lcd.setCursor(0, 1);
    lcd.print("Error");
    return;
  }

  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));

  if (strstr(status, "200") == NULL) {
    lcd.setCursor(0, 0);
    lcd.print("Connection");
    lcd.setCursor(0, 1);
    lcd.print("Error");
    return;
  }

  while (client.available() && client.peek() != '{' && client.peek() != '[') {
    char c = 0;
    client.readBytes(&c, 1);
  }

  while (client.available()) {
    String payload = client.readString();
    DynamicJsonDocument doc(43000);
    DeserializationError error = deserializeJson(doc, payload);
    JsonObject root = doc.as<JsonObject>();

    if(apiIndex == 0) {
      makeSoccerRequest(root);
    }

    if(apiIndex == 1) {
      makeMlbRequest(root);
    }

    if(apiIndex == 2) {
      makeNhlRequest(root);
    }

    if(apiIndex == 3) {
      makeNflRequest(root);
    }
  }
}

void makeSoccerRequest(JsonObject root) {
    JsonArray matches = root["matches"].as<JsonArray>();
    JsonObject lastMatch = matches[0].as<JsonObject>();

    String homeName = lastMatch["homeTeam"]["shortName"].as<String>().substring(0,13);
    String awayName = lastMatch["awayTeam"]["shortName"].as<String>().substring(0,13);

    int homeScore = lastMatch["score"]["fullTime"]["home"].as<int>();
    int awayScore = lastMatch["score"]["fullTime"]["away"].as<int>();

    printScores(homeName, awayName, homeScore, awayScore);
}

void makeMlbRequest(JsonObject root) {
  JsonArray dates = root["dates"].as<JsonArray>();
  JsonObject game = dates[0]["games"][0].as<JsonObject>();
  JsonObject teams = game["teams"].as<JsonObject>();
  JsonObject home = teams["home"].as<JsonObject>();
  JsonObject away = teams["away"].as<JsonObject>();

  int homeScore = home["score"].as<int>();
  int awayScore = away["score"].as<int>();

  String homeName = home["team"]["name"].as<String>();
  homeName = homeName.substring(homeName.lastIndexOf(" ") + 1);
  String awayName = away["team"]["name"].as<String>().substring(awayName.lastIndexOf(" ") + 1);
  awayName = awayName.substring(awayName.lastIndexOf(" ") + 1);

  printScores(homeName, awayName, homeScore, awayScore);
}

void makeNhlRequest(JsonObject root) {
  JsonArray gamesByDate = root["gamesByDate"].as<JsonArray>();
  int lastGameIndex = 0;
  bool foundLastGame = false;
  while(!foundLastGame) {
    JsonObject currentGame = gamesByDate[lastGameIndex].as<JsonObject>();
    String gameState = currentGame["games"][0]["gameState"].as<String>();

    if(gameState.equals("FUT")) {
      lastGameIndex = lastGameIndex - 1;
      JsonObject lastGame = gamesByDate[lastGameIndex]["games"][0].as<JsonObject>();
      JsonObject homeTeam = lastGame["homeTeam"].as<JsonObject>();
      JsonObject awayTeam = lastGame["awayTeam"].as<JsonObject>();

      int homeScore = homeTeam["score"].as<int>();
      int awayScore = awayTeam["score"].as<int>();

      String homeName = homeTeam["abbrev"].as<String>();
      String awayName = awayTeam["abbrev"].as<String>();

      printScores(homeName, awayName, homeScore, awayScore);
      foundLastGame = true;
    }
    lastGameIndex++;
  }
}

void makeNflRequest(JsonObject root) {
  JsonArray schedule = root["body"]["schedule"].as<JsonArray>();

  int lastGameIndex = 0;

  while(lastGameIndex < schedule.size()) {
    JsonObject currentGame = schedule[lastGameIndex].as<JsonObject>();
    String gameStatus = currentGame["gameStatus"].as<String>();

    if(!gameStatus.equals("Completed") && !gameStatus.equals("Live - In Progress") && !gameStatus.equals("Postponed") && !gameStatus.equals("Suspended")) {
      lastGameIndex = lastGameIndex - 1;
      JsonObject lastGame = currentGame;

      int homeScore = lastGame["homePts"].as<int>();
      int awayScore = lastGame["awayPts"].as<int>();

      String homeName = lastGame["home"].as<String>();
      String awayName = lastGame["away"].as<String>();

      printScores(homeName, awayName, homeScore, awayScore);
      return;
    }
    lastGameIndex++;

    if(lastGameIndex == schedule.size()) {
      lastGameIndex = lastGameIndex - 1;
      JsonObject lastGame = currentGame;

      int homeScore = lastGame["homePts"].as<int>();
      int awayScore = lastGame["awayPts"].as<int>();

      String homeName = lastGame["home"].as<String>();
      String awayName = lastGame["away"].as<String>();

      printScores(homeName, awayName, homeScore, awayScore);
      return;
    }
  }
}
