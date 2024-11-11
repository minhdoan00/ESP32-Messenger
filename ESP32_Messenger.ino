#include <WiFiManager.h>  // Thêm thư viện WiFiManager cho ESP32
// Load Wi-Fi library
#include "WiFi.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>  // Thêm thư viện ArduinoJson

// ChatGPT API token
const char* chatgpt_token = "CHATGPT_API";

char chatgpt_server[] = "https://api.openai.com/v1/completions";

// Set web server port number to 80
WiFiServer server(80);
WiFiClient client1;

HTTPClient https;

String chatgpt_Q;
String chatgpt_A;
String json_String;
uint16_t dataStart = 0;
uint16_t dataEnd = 0;
String dataStr;
int httpCode = 0;

String chatHistory = "<div class=\"message bot\">Hello! How can I assist you today?</div>\r\n";

bool initialMessageDisplayed = true;

typedef enum {
  do_webserver_index,
  send_chatgpt_request,
  get_chatgpt_list,
} STATE_;

STATE_ currentState;

// Decode URL-encoded characters
String decodeURIComponent(String input) {
  input.replace("%20", " ");
  input.replace("%21", "!");
  input.replace("%22", "\"");
  input.replace("%23", "#");
  input.replace("%24", "$");
  input.replace("%25", "%");
  input.replace("%26", "&");
  input.replace("%27", "'");
  input.replace("%28", "(");
  input.replace("%29", ")");
  input.replace("%2A", "*");
  return input;
}

void WiFiConnect(void) {
  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP32-ChatGPT-AP");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  currentState = do_webserver_index;
}

String generateHTMLPage() {
  String html = "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n"
                "\r\n"
                "<!DOCTYPE HTML>\r\n"
                "<html lang=\"en\">\r\n"
                "<head>\r\n"
                "  <meta charset=\"UTF-8\">\r\n"
                "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
                "  <title>ESP32 Messenger</title>\r\n"
                "  <link rel=\"icon\" href=\"https://files.seeedstudio.com/wiki/xiaoesp32c3-chatgpt/chatgpt-logo.png\" type=\"image/x-icon\">\r\n"
                "  <style>\r\n"
                "    body { font-family: Arial, sans-serif; background-color: #f0f0f0; color: #333; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }\r\n"
                "    .chat-container { width: 100%; max-width: 500px; background-color: #ffffff; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); border-radius: 8px; padding: 20px; display: flex; flex-direction: column; }\r\n"
                "    .chat-header { font-size: 1.5em; color: #007acc; margin-bottom: 20px; text-align: center; }\r\n"
                "    .chat-box { flex-grow: 1; overflow-y: auto; margin-bottom: 20px; padding: 10px; border: 1px solid #ccc; border-radius: 4px; background-color: #fafafa; height: 300px; }\r\n"
                "    .message { margin: 10px 0; padding: 10px; border-radius: 8px; }\r\n"
                "    .message.user { background-color: #e0f7fa; text-align: right; }\r\n"
                "    .message.bot { background-color: #e0e0e0; text-align: left; }\r\n"
                "    form { display: flex; }\r\n"
                "    input[type=text] { flex-grow: 1; padding: 10px; margin-right: 10px; border: 1px solid #ccc; border-radius: 4px; }\r\n"
                "    input[type=submit] { background-color: #007acc; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; transition: background-color 0.3s; }\r\n"
                "    input[type=submit]:hover { background-color: #005f99; }\r\n"
                "  </style>\r\n"
                "  <script>\r\n"
                "    function sendMessage(event) {\r\n"
                "      event.preventDefault();\r\n"
                "      const chatbox = document.getElementById('chatbox');\r\n"
                "      const userInput = document.querySelector('input[name=\"chatgpttext\"]').value;\r\n"
                "      const message = document.createElement('div');\r\n"
                "      message.classList.add('message', 'user');\r\n"
                "      message.textContent = userInput;\r\n"
                "      chatbox.appendChild(message);\r\n"
                "      document.querySelector('input[name=\"chatgpttext\"]').value = '';\r\n"
                "      fetch('/', {\r\n"
                "        method: 'POST',\r\n"
                "        headers: {\r\n"
                "          'Content-Type': 'application/x-www-form-urlencoded'\r\n"
                "        },\r\n"
                "        body: `chatgpttext=${encodeURIComponent(userInput)}`\r\n"
                "      }).then(response => response.text())\r\n"
                "        .then(html => {\r\n"
                "          setTimeout(() => {\r\n"
                "            fetch('/')\r\n"
                "              .then(response => response.text())\r\n"
                "              .then(htmlContent => {\r\n"
                "                const tempDiv = document.createElement('div');\r\n"
                "                tempDiv.innerHTML = htmlContent;\r\n"
                "                const newMessages = tempDiv.querySelector('.chat-box').innerHTML;\r\n"
                "                chatbox.innerHTML += newMessages;\r\n"
                "                chatbox.scrollTop = chatbox.scrollHeight;\r\n"
                "              });\r\n"
                "          }, 2000);\r\n"
                "        });\r\n"
                "    }\r\n"
                "  </script>\r\n"
                "</head>\r\n"
                "<body>\r\n"
                "  <div class=\"chat-container\">\r\n"
                "    <div class=\"chat-header\">ESP32 Messenger</div>\r\n"
                "    <div class=\"chat-box\" id=\"chatbox\">\r\n" + chatHistory +
                "    </div>\r\n"
                "    <form onsubmit=\"sendMessage(event)\">\r\n"
                "      <input type=\"text\" placeholder=\"Type your message...\" name=\"chatgpttext\" required=\"required\"/>\r\n"
                "      <input type=\"submit\" value=\"Send\"/>\r\n"
                "    </form>\r\n"
                "  </div>\r\n"
                "</body>\r\n"
                "</html>\r\n";

  return html;
}

void setup() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  while (!Serial);

  Serial.println("WiFi Setup done!");

  WiFiConnect();
  // Start the TCP server server
  server.begin();
}

void loop() {
  switch (currentState) {
    case do_webserver_index:
      Serial.println("Web Production Task Launch");
      client1 = server.available();  // Check if the client is connected
      if (client1) {
        Serial.println("New Client.");  // print a message out the serial port
        boolean currentLineIsBlank = true;
        while (client1.connected()) {
          if (client1.available()) {
            char c = client1.read();  // Read the rest of the content, used to clear the cache
            json_String += c;
            if (c == '\n' && currentLineIsBlank) {
              dataStr = json_String.substring(0, 4);
              Serial.println(dataStr);
              if (dataStr == "GET ") {
                String htmlContent = generateHTMLPage();
                client1.print(htmlContent);  // Send the response body to the client
                client1.stop();  // Close the connection
              } else if (dataStr == "POST") {
                json_String = "";
                while (client1.available()) {
                  json_String += (char)client1.read();
                }
                Serial.println(json_String);
                dataStart = json_String.indexOf("chatgpttext=") + strlen("chatgpttext=");
                chatgpt_Q = json_String.substring(dataStart, json_String.length());
                chatgpt_Q = decodeURIComponent(chatgpt_Q);  // Decode URL-encoded characters
                Serial.print("Your Question is: ");
                Serial.println(chatgpt_Q);

                // Gửi phản hồi HTTP cho trình duyệt ngay lập tức
                client1.println("HTTP/1.1 200 OK");
                client1.println("Content-Type: text/html");
                client1.println("Connection: close");
                client1.println();
                client1.println("<html><body><p style=\"font-weight: normal;\"><em>Message received. Processing...</em></p></body></html>");

                // Đóng kết nối
                delay(10);
                client1.stop();

                // Chuyển sang trạng thái gửi yêu cầu ChatGPT
                currentState = send_chatgpt_request;
              }
              json_String = "";
              break;
            }
            if (c == '\n') {
              currentLineIsBlank = true;
            } else if (c != '\r') {
              currentLineIsBlank = false;
            }
          }
        }
      }
      delay(1000);
      break;

    case send_chatgpt_request:
      Serial.println("Ask ChatGPT a Question Task Launch");
      if (https.begin(chatgpt_server)) {  // HTTPS
        https.addHeader("Content-Type", "application/json");
        String token_key = String("Bearer ") + chatgpt_token;
        https.addHeader("Authorization", token_key);
        String payload = String("{\"model\": \"gpt-3.5-turbo-instruct\", \"prompt\": \"") + chatgpt_Q + String("\", \"temperature\": 0, \"max_tokens\": 100}");
        httpCode = https.POST(payload);  // Start connection and send HTTP header
        payload = "";
        currentState = get_chatgpt_list;
      } else {
        Serial.println("[HTTPS] Unable to connect");
        delay(1000);
      }
      break;
    case get_chatgpt_list:
      Serial.println("Get ChatGPT Answers Task Launch");
      // httpCode will be negative on error
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        Serial.println(payload);  // Print entire response for debugging

        // Parse JSON response to get the answer
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          chatgpt_A = "Error parsing response from ChatGPT.";
        } else {
          chatgpt_A = doc["choices"][0]["text"].as<String>();
          Serial.print("ChatGPT Answer is: ");
          Serial.println(chatgpt_A);
        }

        // Update chat history with new response
        chatHistory = "<div class=\"message bot\">" + chatgpt_A + "</div>\r\n";

        Serial.println("Wait 2s before next round...");
        currentState = do_webserver_index;
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        currentState = do_webserver_index; // Retry instead of halting
      }
      https.end();
      delay(2000);
      break;
  }
}
