#include <Arduino.h>
#include <WiFi.h>                   
#include <SPIFFS.h>                 
#include <AsyncTCP.h>              
#include <ESPAsyncWebServer.h>      

const char* ssid = "RedmiNuria";     // SSID de tu red WiFi
const char* password = "Patata123"; // Contraseña de tu red WiFi

AsyncWebServer server(80);          // Crea un servidor web en el puerto 80

// Contenido HTML de la página web para gestionar archivos
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 File Manager</title>
  <style>
    body { font-family: Arial; }
    ul { list-style-type: none; }
    #fileInput { display: none; }
  </style>
</head>
<body>
  <h2>ESP32 File Manager</h2>
  <button onclick="document.getElementById('fileInput').click();">Subir Archivo</button>
  <input type="file" id="fileInput" onchange="uploadFile()" multiple>
  <ul id="fileList"></ul>

  <script>
  document.addEventListener('DOMContentLoaded', function (e) {
    // Al cargar la página, solicita la lista de archivos disponibles al servidor
    fetch('/list').then(response => response.json())
      .then(data => {
        var fileList = document.getElementById('fileList');
        data.forEach(file => {
          var li = document.createElement('li');
          var a = document.createElement('a');
          a.href = `/download/${file}`;
          a.text = file;
          li.appendChild(a);
          fileList.appendChild(li);
        });
      });
  });

  // Función para cargar archivos al servidor
  function uploadFile() {
    var files = document.getElementById('fileInput').files;
    var formData = new FormData();
    for (var i = 0; i < files.length; i++) {
      formData.append('file', files[i]);
    }

    // Realiza una solicitud POST para cargar archivos
    fetch('/upload', {method: 'POST', body: formData})
      .then(response => {
        if (response.ok) {
          console.log('Upload successful');
          location.reload();  // Recarga la página después de una carga exitosa
        } else {
          console.error('Upload failed');
        }
      });
  }
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);   // Inicializa la comunicación serial para la depuración
  if(!SPIFFS.begin(true)){ // Intenta montar el sistema de archivos SPIFFS
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
  }

  WiFi.begin(ssid, password);   // Conéctate a la red WiFi especificada
  while (WiFi.status() != WL_CONNECTED) { // Espera hasta que la conexión se establezca
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  // Configuración de rutas del servidor web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html); // Envía la página HTML al cliente
  });
  Serial.println(WiFi.localIP());


  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    // Devuelve la lista de archivos almacenados en SPIFFS en formato JSON
    String json = "[";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file){
        if(json != "[") json += ',';
        json += "\"" + String(file.name()) + "\"";
        file = root.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
    file.close();
  });

  // Sube archivos al sistema de archivos SPIFFS
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){},
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if(!index){
        Serial.printf("UploadStart: %s\n", filename.c_str());
        request->_tempFile = SPIFFS.open("/" + filename, "w");
      }
      if(len){
        request->_tempFile.write(data, len);
      }
      if(final){
        Serial.printf("UploadEnd: %s(%u)\n", filename.c_str(), index+len);
        request->_tempFile.close();
      }
  });

  // Descarga archivos del sistema de archivos SPIFFS
  server.on("^\\/download\\/(.*)$", HTTP_GET, [](AsyncWebServerRequest *request){
    String filename = request->pathArg(0);  // Obtiene el nombre del archivo de la URL
    Serial.println("Download: " + filename);
    if(SPIFFS.exists("/" + filename)){  // Verifica si el archivo existe en SPIFFS
      request->send(SPIFFS, "/" + filename, String(), true); // Envía el archivo al cliente
    } else {
      request->send(404); // Envía un código de error 404 si el archivo no existe
    }
  });

  server.begin(); // Inicia el servidor web
}

void loop() {
  // El loop está vacío ya que todas las operaciones se realizan en los controladores de eventos del servidor web
}
