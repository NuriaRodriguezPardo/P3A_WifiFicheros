#include <Arduino.h>
#include <WiFi.h>                   
#include <SPIFFS.h>                 
#include <AsyncTCP.h>              
#include <ESPAsyncWebServer.h>      

const char* ssid = "RedmiNuria";
const char* password = "Patata123";

AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 File Manager</title>
  <style>
    body {
      font-family: Arial;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      background-color: #f0f0f0;
    }
    .container {
      width: 60%;
      background-color: #fff;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
    }
    h2 { text-align: center; }
    table { width: 100%; border-collapse: collapse; }
    th, td { border: 1px solid #dddddd; text-align: left; padding: 8px; }
    th { background-color: #f2f2f2; }
    button { background-color: #4CAF50; color: white; padding: 5px 10px; border: none; border-radius: 4px; cursor: pointer; }
    button:hover { background-color: #45a049; }
    .action-column { background-color: #f9f9f9; }
    .console { margin-top: 20px; padding: 10px; background-color: #e6e6e6; border-radius: 5px; }
  </style>
</head>
<body>
  <div class="container">
    <h2>ESP32 File Manager</h2>
    <button onclick="document.getElementById('fileInput').click();">Subir Archivo</button>
    <input type="file" id="fileInput" style="display:none;" onchange="uploadFile()" multiple>
    <br><br>
    <table>
      <thead>
        <tr>
          <th>Nombre de Archivo</th>
          <th class="action-column">Acciones</th>
        </tr>
      </thead>
      <tbody id="fileList"></tbody>
    </table>
    <div class="console" id="console"></div>
  </div>

  <script>
  function writeToConsole(message) {
    var consoleDiv = document.getElementById('console');
    consoleDiv.innerHTML += message + '<br>';
    consoleDiv.scrollTop = consoleDiv.scrollHeight;
  }

  document.addEventListener('DOMContentLoaded', function (e) {
    writeToConsole('Pagina cargada');

    fetch('/list').then(response => response.json())
      .then(data => {
        var fileList = document.getElementById('fileList');
        data.forEach(file => {
          var tr = document.createElement('tr');
          var tdName = document.createElement('td');
          var tdActions = document.createElement('td');
          var aDownload = document.createElement('button');
          var aDelete = document.createElement('button');

          aDownload.textContent = 'Descargar';
          aDownload.onclick = function() {
            writeToConsole('Descargando ' + file);
            window.location = '/download/' + file;
          };

          aDelete.textContent = 'Eliminar';
          aDelete.onclick = function() {
            writeToConsole('Eliminando ' + file);
            deleteFile(file);
          };

          tdName.textContent = file;
          tdActions.appendChild(aDownload);
          tdActions.appendChild(aDelete);
          
          tr.appendChild(tdName);
          tr.appendChild(tdActions);
          
          fileList.appendChild(tr);
        });
      });
  });

  function uploadFile() {
    var files = document.getElementById('fileInput').files;
    var formData = new FormData();
    for (var i = 0; i < files.length; i++) {
      formData.append('file', files[i]);
    }

    fetch('/upload', {method: 'POST', body: formData})
      .then(response => {
        if (response.ok) {
          writeToConsole('Archivo(s) subido(s) con éxito');
          location.reload();
        } else {
          writeToConsole('Error al subir archivo(s)');
          console.error('Upload failed');
        }
      });
  }

  function deleteFile(filename) {
    fetch('/delete', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'filename=' + filename
    })
    .then(response => {
      if (response.ok) {
        writeToConsole('Archivo eliminado con éxito');
        location.reload();
      } else {
        writeToConsole('Error al eliminar archivo');
        console.error('Delete failed');
      }
    });
  }
  </script>
</body>
</html>
)rawliteral";



void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  Serial.println(WiFi.localIP());


  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "[";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
      if (json != "[") json += ',';
      json += "\"" + String(file.name()) + "\"";
      file = root.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
    file.close();
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
  },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
              if (!index) {
                Serial.printf("UploadStart: %s\n", filename.c_str());
                request->_tempFile = SPIFFS.open("/" + filename, "w");
              }
              if (len) {
                request->_tempFile.write(data, len);
              }
              if (final) {
                Serial.printf("UploadEnd: %s(%u)\n", filename.c_str(), index + len);
                request->_tempFile.close();
              }
            });
            
  server.on("^\\/download\\/(.*)$", HTTP_GET, [](AsyncWebServerRequest *request) {
    String filename = request->pathArg(0);
    Serial.println("Download: " + filename);
    if (SPIFFS.exists("/" + filename)) {
      request->send(SPIFFS, "/" + filename, String(), true);
    } else {
      request->send(404);
    }
  });

  server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("filename", true)) {
        AsyncWebParameter* p = request->getParam("filename", true);
        String filename = "/" + p->value();
        if (SPIFFS.exists(filename)) {
            if (SPIFFS.remove(filename)) {
                request->send(200, "text/plain", "File deleted successfully");
                Serial.println("File deleted: " + filename); // Mensaje en la terminal
            } else {
                request->send(500, "text/plain", "Failed to delete file");
            }
        } else {
            request->send(404, "text/plain", "File not found");
        }
    } else {
        request->send(400, "text/plain", "Missing filename parameter");
    }
});


  server.begin();
}

void loop() {
}
