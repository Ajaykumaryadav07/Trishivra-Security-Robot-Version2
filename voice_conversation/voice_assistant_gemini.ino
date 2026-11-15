/*
===================================================================== TRISHIVRA ESP32-S3 VOICE ASSISTANT FULL GROQ VERSION ===================================================================== PIPELINE: Push button GPIO7 ↓ INMP441 microphone ↓ ESP32-S3 records 16 kHz PCM16 into PSRAM ↓ WAV created in PSRAM ↓ Groq Whisper Large V3 Turbo ↓ Speech-To-Text ↓ Groq GPT-OSS 20B ↓ Short text response ↓ Groq Orpheus English TTS ↓ 24 kHz WAV ↓ MAX98357A ↓ Speaker HARDWARE ======== INMP441: VDD -> 3.3V GND -> GND SCK -> GPIO4 WS  -> GPIO5 SD  -> GPIO6 L/R -> GND Push button: GPIO7 -> button -> GND MAX98357A: VIN  -> 5V preferred GND  -> GND BCLK -> GPIO15 LRC  -> GPIO16 DIN  -> GPIO17 Speaker: Speaker + -> MAX98357A + Speaker - -> MAX98357A - =====================================================================
*/
// =================================================================
// LIBRARIES
// =================================================================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "driver/i2s_std.h"
#include <LittleFS.h>
#include "Audio.h"
// =================================================================
// WIFI
// =================================================================
const char* WIFI_SSID = "Shubham";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
// =================================================================
// GROQ API KEY
// =================================================================
// IMPORTANT:
// Put your own Groq key here.
// Never upload this key to GitHub.
const char* GROQ_API_KEY = "YOUR_GROQ_API_KEY";
// =================================================================
// GROQ MODELS
// =================================================================
// Speech-To-Text
const char* GROQ_STT_MODEL = "whisper-large-v3-turbo";
// LLM
const char* GROQ_LLM_MODEL = "openai/gpt-oss-20b";
// Text-To-Speech
const char* GROQ_TTS_MODEL = "canopylabs/orpheus-v1-english";
// TTS voice
const char* GROQ_TTS_VOICE = "troy";
// =================================================================
// PUSH BUTTON
// =================================================================
#define PTT_BUTTON 7
// =================================================================
// INMP441
// =================================================================
#define MIC_BCLK 4
#define MIC_WS   5
#define MIC_DATA 6
// =================================================================
// MAX98357A
// =================================================================
#define SPK_BCLK 15
#define SPK_LRC  16
#define SPK_DATA 17
// =================================================================
// AUDIO SETTINGS
// =================================================================
#define MIC_SAMPLE_RATE 16000
// We explicitly request 24 kHz from Groq TTS.
#define SPEAKER_SAMPLE_RATE 24000
#define MAX_RECORD_SECONDS 15
#define MAX_AUDIO_SAMPLES \
  (MIC_SAMPLE_RATE * MAX_RECORD_SECONDS)
// =================================================================
// I2S HANDLES
// =================================================================
i2s_chan_handle_t micRX = nullptr;
i2s_chan_handle_t speakerTX = nullptr;
Audio audio;
// =================================================================
// RECORDING BUFFER
// =================================================================
int16_t* audioBuffer = nullptr;
size_t audioSamples = 0;
// =================================================================
// RECORDING STATE
// =================================================================
bool recording = false;
bool previousButtonState = HIGH;
// =================================================================
// FUNCTION DECLARATIONS
// =================================================================

void connectWiFi();
bool setupMicrophone();
bool setupSpeaker();
void startRecording();
void recordAudio();int16_t* audioBuffer = nullptr;

void playLocalMP3(const char* filename);
String matchIntent(String question);
void stopRecording();
void writeLE16( uint8_t* buffer, int offset, uint16_t value );
void writeLE32( uint8_t* buffer, int offset, uint32_t value );
uint16_t readLE16( const uint8_t* buffer, size_t offset );
uint32_t readLE32( const uint8_t* buffer, size_t offset );
uint8_t* createWav( int16_t* pcm, size_t samples, size_t& wavSize );
String transcribeGroq( uint8_t* wav, size_t wavSize );
String askGroq( const String& question );
bool speakGroq( const String& text );
bool playWavFromMemory( uint8_t* wav, size_t wavSize );
bool playLocalWav( const char* filename );
String jsonEscape( const String& input );
// =================================================================
// SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("========================================");
  Serial.println(" TRISHIVRA VOICE ASSISTANT");
  Serial.println(" FULL GROQ VERSION");
  Serial.println(" STT -> LLM -> TTS");
  Serial.println("========================================");
  Serial.println();
  // -------------------------------------------------------------
  // Push button
  // -------------------------------------------------------------
  pinMode( PTT_BUTTON, INPUT_PULLUP );
   // -------------------------------------------------------------
   // LittleFS
  // -------------------------------------------------------------
  if (!LittleFS.begin(true)) {
  Serial.println("[ERROR] LittleFS mount failed");
  while (true) {
      delay(1000);
  }
  }
  Serial.println("[OK] LittleFS mounted");
  // -------------------------------------------------------------
  // PSRAM
  // -------------------------------------------------------------
  if (!psramFound()) {
      Serial.println( "[ERROR] PSRAM NOT detected!" );
      while (true) {
          delay(1000);
      }
  }
  Serial.println( "[OK] PSRAM detected" );
  Serial.print( "[INFO] Free PSRAM: " );
  Serial.println( ESP.getFreePsram() );
  // -------------------------------------------------------------
  // Allocate recording buffer
  // -------------------------------------------------------------
  audioBuffer = (int16_t*)ps_malloc( MAX_AUDIO_SAMPLES * sizeof(int16_t) );
  if (audioBuffer == nullptr) {
      Serial.println( "[ERROR] Audio buffer allocation failed" );
      while (true) {
          delay(1000);
      }
  }
  Serial.print( "[OK] Recording buffer: " );
  Serial.print( MAX_AUDIO_SAMPLES * sizeof(int16_t) );
  Serial.println( " bytes" );
  // -------------------------------------------------------------
  // Microphone
  // -------------------------------------------------------------
  if (!setupMicrophone()) {
      Serial.println( "[ERROR] Microphone setup failed" );
      while (true) {
          delay(1000);
      }
  }
  Serial.println( "[OK] INMP441 ready" );
  // -------------------------------------------------------------
  // Speaker
  // -------------------------------------------------------------
  if (!setupSpeaker()) {
      Serial.println( "[ERROR] Speaker setup failed" );
      while (true) {
          delay(1000);
      }
  }
  Serial.println( "[OK] MAX98357A ready" );
  delay(1000);
  audio.setPinout(SPK_BCLK, SPK_LRC, SPK_DATA);
  audio.setVolume(21);
  playLocalWav("/bbd_location_24k.wav");
  // -------------------------------------------------------------
  // WiFi
  // -------------------------------------------------------------
  connectWiFi();
  Serial.println();
  Serial.println("========================================");
  Serial.println(" READY");
  Serial.println();
  Serial.println(" HOLD GPIO7 BUTTON -> SPEAK");
  Serial.println(" RELEASE BUTTON    -> PROCESS");
  Serial.println("========================================");
  Serial.println();
}
// =================================================================
// MAIN LOOP
// =================================================================
void loop() {
  bool buttonState = digitalRead( PTT_BUTTON );
  // -------------------------------------------------------------
  // BUTTON PRESSED
  // -------------------------------------------------------------
  if ( previousButtonState == HIGH && buttonState == LOW ) {
      startRecording();
  }
  // -------------------------------------------------------------
  // RECORD
  // -------------------------------------------------------------
  if ( recording && buttonState == LOW ) {
      recordAudio();
  }
  // -------------------------------------------------------------
  // BUTTON RELEASED
  // -------------------------------------------------------------
  if ( previousButtonState == LOW && buttonState == HIGH ) {
      if (recording) {
          stopRecording();
          // Ignore very short presses
          if ( audioSamples > (MIC_SAMPLE_RATE / 4) ) {
              size_t wavSize = 0;
              uint8_t* wav = createWav( audioBuffer, audioSamples, wavSize );
              if (wav != nullptr) {
                  Serial.println();
                  Serial.println( "========================================" );
                  // =================================================
                  // STAGE 1
                  // =================================================
                  Serial.println( "[1/3] GROQ STT" );
                  Serial.println( "[STT] Whisper Large V3 Turbo..." );
                  String question = transcribeGroq( wav, wavSize );
                  // WAV no longer needed after STT
                  free(wav);
                  if ( question.length() > 0 ) {
                      Serial.println();
                      Serial.print( "YOU: " );
                      Serial.println( question );
                      // =============================================
                      // STAGE 2
                      // =============================================
                        
                      String mp3File = matchIntent(question);
if (mp3File.length() > 0) {
    Serial.print("[MATCH] Playing local: ");
    Serial.println(mp3File);
    playLocalMP3(mp3File.c_str());
} else {
    // Fallback to online LLM + TTS
       Serial.println();
       Serial.println( "[2/3] GROQ LLM" );
       String answer = askGroq(question);
    // ... existing code
}  

                      //Serial.println();
                     // Serial.println( "[2/3] GROQ LLM" );
                      //String answer = askGroq( question );
                      if ( answer.length() > 0 ) {
                          Serial.println();
                          Serial.print( "TRISHIVRA: " );
                          Serial.println( answer );
                          // =========================================
                          // STAGE 3
                          // =========================================
                          Serial.println();
                          Serial.println( "[3/3] GROQ TTS" );
                          if ( !speakGroq( answer ) ) {
                              Serial.println( "[ERROR] TTS failed" );
                          }
                      }
                      else
                      {
                          Serial.println( "[ERROR] Groq LLM gave no answer" );
                      }
                  }
                  else
                  {
                      Serial.println( "[ERROR] STT returned no text" );
                  }
              }
          }
          else
          {
              Serial.println( "[INFO] Recording too short" );
          }
          Serial.println();
          Serial.println( "========================================" );
          Serial.println( "Ready for next question..." );
          Serial.println( "========================================" );
          Serial.println();
      }
  }
  previousButtonState = buttonState;
  delay(2);
}
// =================================================================
// WIFI
// =================================================================
void connectWiFi() {
  Serial.println();
  Serial.print( "[WiFi] Connecting to " );
  Serial.println( WIFI_SSID );
  WiFi.mode( WIFI_STA );
  WiFi.begin( WIFI_SSID, WIFI_PASSWORD );
  int attempts = 0;
  while ( WiFi.status() != WL_CONNECTED && attempts < 40 ) {
      Serial.print(".");
      delay(500);
      attempts++;
  }
  Serial.println();
  if ( WiFi.status() == WL_CONNECTED ) {
      Serial.println( "[WiFi] Connected" );
      Serial.print( "[WiFi] IP: " );
      Serial.println( WiFi.localIP() );
  }
  else
  {
      Serial.println( "[ERROR] WiFi failed" );
  }
}
// =================================================================
// MICROPHONE SETUP
// =================================================================
bool setupMicrophone() {
  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG( I2S_NUM_0, I2S_ROLE_MASTER );
  esp_err_t result = i2s_new_channel( &channelConfig, nullptr, &micRX );
  if ( result != ESP_OK ) {
      Serial.print( "[MIC] New channel error: " );
      Serial.println( result );
      return false;
  }
  i2s_std_config_t config = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG( MIC_SAMPLE_RATE ), .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG( I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO ), .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED, .bclk = (gpio_num_t)MIC_BCLK, .ws = (gpio_num_t)MIC_WS, .dout = I2S_GPIO_UNUSED, .din = (gpio_num_t)MIC_DATA, .invert_flags = {
              .mclk_inv = false, .bclk_inv = false, .ws_inv = false }
      }
  };
  // INMP441 L/R -> GND = LEFT
  config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  result = i2s_channel_init_std_mode( micRX, &config );
  if ( result != ESP_OK ) {
      return false;
  }
  result = i2s_channel_enable( micRX );   
  return ( result == ESP_OK );
}
// =================================================================
// SPEAKER SETUP
// =================================================================
bool setupSpeaker() {
  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG( I2S_NUM_1, I2S_ROLE_MASTER );
  esp_err_t result = i2s_new_channel( &channelConfig, &speakerTX, nullptr );
  if ( result != ESP_OK ) {
      Serial.print( "[SPK] New channel error: " );
      Serial.println( result );
      return false;
  }
  i2s_std_config_t config = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG( SPEAKER_SAMPLE_RATE ), .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG( I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO ), .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED, .bclk = (gpio_num_t)SPK_BCLK, .ws = (gpio_num_t)SPK_LRC, .dout = (gpio_num_t)SPK_DATA, .din = I2S_GPIO_UNUSED, .invert_flags = {
              .mclk_inv = false, .bclk_inv = false, .ws_inv = false }
      }
  };
  config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  result = i2s_channel_init_std_mode( speakerTX, &config );
  if ( result != ESP_OK ) {
      return false;
  }
  result = i2s_channel_enable( speakerTX );
  return ( result == ESP_OK );
}
// =================================================================
// START RECORDING
// =================================================================
void startRecording() {
  audioSamples = 0;
  recording = true;
  Serial.println();
  Serial.println( "========================================" );
  Serial.println( "RECORDING..." );
  Serial.println( "Speak now." );
}
// =================================================================
// RECORD AUDIO
// =================================================================
void recordAudio() {
  if (!recording) {
      return;
  }
  if ( audioSamples >= MAX_AUDIO_SAMPLES ) {
      return;
  }
  int32_t rawBuffer[256];
  size_t bytesRead = 0;
  esp_err_t result = i2s_channel_read( micRX, rawBuffer, sizeof(rawBuffer), &bytesRead, 100 );
  if ( result != ESP_OK ) {
      return;
  }
  size_t samplesRead = bytesRead / sizeof(int32_t);
  for ( size_t i = 0; i < samplesRead; i++ ) {
      if ( audioSamples >= MAX_AUDIO_SAMPLES ) {
          break;
      }
      int32_t sample = rawBuffer[i];
      // INMP441 32-bit sample -> PCM16
      sample >>= 14;
      if ( sample > 32767 ) {
          sample = 32767;
      }
      if ( sample < -32768 ) {
          sample = -32768;
      }
      audioBuffer[ audioSamples++ ] = (int16_t)sample;
  }
}
// =================================================================
// STOP RECORDING
// =================================================================
void stopRecording() {
  recording = false;
  float seconds = (float)audioSamples / MIC_SAMPLE_RATE;
  Serial.println();
  Serial.println( "RECORDING STOPPED" );
  Serial.print( "[INFO] Samples: " );
  Serial.println( audioSamples );
  Serial.print( "[INFO] Recorded seconds: " );
  Serial.println( seconds, 2 );
}
// =================================================================
// LITTLE-ENDIAN WRITE
// =================================================================
void writeLE16( uint8_t* buffer, int offset, uint16_t value ) {
  buffer[offset] = value & 0xFF;
  buffer[offset + 1] = (value >> 8) & 0xFF;
}
void writeLE32( uint8_t* buffer, int offset, uint32_t value ) {
  buffer[offset] = value & 0xFF;
  buffer[offset + 1] = (value >> 8) & 0xFF;
  buffer[offset + 2] = (value >> 16) & 0xFF;
  buffer[offset + 3] = (value >> 24) & 0xFF;
}
// =================================================================
// LITTLE-ENDIAN READ
// =================================================================
uint16_t readLE16( const uint8_t* buffer, size_t offset ) {
  return (uint16_t)buffer[offset] | ((uint16_t)buffer[offset + 1] << 8);
}
uint32_t readLE32( const uint8_t* buffer, size_t offset ) {
  return (uint32_t)buffer[offset] | ((uint32_t)buffer[offset + 1] << 8) | ((uint32_t)buffer[offset + 2] << 16) | ((uint32_t)buffer[offset + 3] << 24);
}
// =================================================================
// CREATE WAV IN PSRAM
// =================================================================
uint8_t* createWav( int16_t* pcm, size_t samples, size_t& wavSize ) {
  const size_t HEADER_SIZE = 44;
  size_t pcmBytes = samples * sizeof(int16_t);
  wavSize = HEADER_SIZE + pcmBytes;
  uint8_t* wav = (uint8_t*)ps_malloc( wavSize );
  if ( wav == nullptr ) {
      Serial.println( "[ERROR] WAV allocation failed" );
      return nullptr;
  }
  // RIFF
  memcpy( wav, "RIFF", 4 );
  writeLE32( wav, 4, wavSize - 8 );
  memcpy( wav + 8, "WAVE", 4 );
  // fmt
  memcpy( wav + 12, "fmt ", 4 );
  writeLE32( wav, 16, 16 );
  // PCM
  writeLE16( wav, 20, 1 );
  // Mono
  writeLE16( wav, 22, 1 );
  // Sample rate
  writeLE32( wav, 24, MIC_SAMPLE_RATE );
  // Byte rate
  writeLE32( wav, 28, MIC_SAMPLE_RATE * 2 );
  // Block align
  writeLE16( wav, 32, 2 );
  // 16 bit
  writeLE16( wav, 34, 16 );
  memcpy( wav + 36, "data", 4 );
  writeLE32( wav, 40, pcmBytes );
  memcpy( wav + HEADER_SIZE, pcm, pcmBytes );
  return wav;
}
// =================================================================
// GROQ SPEECH-TO-TEXT
// =================================================================
//
// Endpoint:
// POST /openai/v1/audio/transcriptions
//
// multipart/form-data:
// file
// model
// response_format
// language
//
// =================================================================
String transcribeGroq( uint8_t* wav, size_t wavSize ) {
  if ( WiFi.status() != WL_CONNECTED ) {
      connectWiFi();
  }
  Serial.println( "[STT] Preparing multipart WAV upload..." );
  String boundary = "----TrishivraBoundary7MA4YWxk";
  // -------------------------------------------------------------
  // Multipart header
  // -------------------------------------------------------------
  String prefix = "";
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; " "name=\"model\"\r\n\r\n";
  prefix += String(GROQ_STT_MODEL) + "\r\n";
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; " "name=\"response_format\"\r\n\r\n";
  prefix += "json\r\n";
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; " "name=\"language\"\r\n\r\n";
  prefix += "en\r\n";
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; " "name=\"file\"; " "filename=\"trishivra.wav\"\r\n";
  prefix += "Content-Type: audio/wav\r\n\r\n";
  String suffix = "";
  suffix += "\r\n--" + boundary + "--\r\n";
  size_t bodySize = prefix.length() + wavSize + suffix.length();
  Serial.print( "[STT] WAV bytes: " );
  Serial.println( wavSize );
  Serial.print( "[STT] HTTP body bytes: " );
  Serial.println( bodySize );
  // -------------------------------------------------------------
  // Allocate multipart request in PSRAM
  // -------------------------------------------------------------
  uint8_t* body = (uint8_t*)ps_malloc( bodySize );
  if ( body == nullptr ) {
      Serial.println( "[ERROR] STT multipart allocation failed" );
      return "";
  }
  size_t offset = 0;
  memcpy( body + offset, prefix.c_str(), prefix.length() );
  offset += prefix.length();
  memcpy( body + offset, wav, wavSize );
  offset += wavSize;
  memcpy( body + offset, suffix.c_str(), suffix.length() );
  // -------------------------------------------------------------
  // HTTPS
  // -------------------------------------------------------------
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = "https://api.groq.com/" "openai/v1/audio/transcriptions";
  if ( !https.begin( client, url ) ) {
      Serial.println( "[ERROR] STT HTTPS begin failed" );
      free(body);
      return "";
  }
  https.addHeader( "Authorization", "Bearer " + String(GROQ_API_KEY) );
  https.addHeader( "Content-Type", "multipart/form-data; boundary=" + boundary );
  https.setTimeout( 60000 );
  Serial.println( "[STT] Sending WAV to Groq..." );
  int httpCode = https.sendRequest( "POST", body, bodySize );
  // Request already sent, free PSRAM
  free(body);
  Serial.print( "[STT HTTP] " );
  Serial.println( httpCode );
  String response = https.getString();
  https.end();
  if ( httpCode < 200 || httpCode >= 300 ) {
      Serial.println();
      Serial.println( "===== GROQ STT ERROR =====" );
      Serial.println( response );
      Serial.println( "==========================" );
      return "";
  }
  // -------------------------------------------------------------
  // Parse JSON
  // -------------------------------------------------------------
  DynamicJsonDocument doc( 8192 );
  DeserializationError error = deserializeJson( doc, response );
  if (error) {
      Serial.print( "[STT] JSON error: " );
      Serial.println( error.c_str() );
      return "";
  }
  String text = doc["text"].as<String>();
  text.trim();
  return text;
}



// =================================================================
// GROQ LLM
// =================================================================
String askGroq( const String& question ) {
  if ( WiFi.status() != WL_CONNECTED ) {
      connectWiFi();
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = "https://api.groq.com/" "openai/v1/chat/completions";
  if ( !https.begin( client, url ) ) {
      Serial.println( "[GROQ] LLM connection failed" );
      return "";
  }
  https.addHeader( "Content-Type", "application/json" );
  https.addHeader( "Authorization", "Bearer " + String(GROQ_API_KEY) );
  https.setTimeout( 60000 );
  // -------------------------------------------------------------
  // Build JSON
  // -------------------------------------------------------------
  DynamicJsonDocument request( 4096 );
  request["model"] = GROQ_LLM_MODEL;
  request["temperature"] = 0.5;
  // GPT-OSS supports low reasoning effort.
  // Lower reasoning is better for a fast voice assistant.
  request["reasoning_effort"] = "low";
  request["reasoning_format"] = "hidden";
  request["max_completion_tokens"] = 200;
  JsonArray messages = request.createNestedArray( "messages" );
  JsonObject user = messages.createNestedObject();
  user["role"] = "user";
  String prompt = "You are Trishivra, a security robot voice assistant. " "Give a direct and accurate answer in one short sentence. " "Use maximum 20 words. " "Question: ";
  prompt += question;
  user["content"] = prompt;
  String body;
  serializeJson( request, body );
  Serial.println( "[GROQ] Asking GPT-OSS 20B..." );
  // -------------------------------------------------------------
  // Request
  // -------------------------------------------------------------
  int httpCode = https.POST( body );
  Serial.print( "[GROQ HTTP] " );
  Serial.println( httpCode );
  String response = https.getString();
  https.end();
  if ( httpCode < 200 || httpCode >= 300 ) {
      Serial.println();
      Serial.println( "===== GROQ LLM ERROR =====" );
      Serial.println( response );
      Serial.println( "==========================" );
      return "";
  }
  // -------------------------------------------------------------
  // Parse answer
  // -------------------------------------------------------------
  DynamicJsonDocument result( 16384 );
  DeserializationError error = deserializeJson( result, response );
  if (error) {
      Serial.print( "[GROQ] JSON error: " );
      Serial.println( error.c_str() );
      return "";
  }
  String answer = result[ "choices" ][0][ "message" ][ "content" ].as<String>();
  answer.trim();
  return answer;
}
// =================================================================
// GROQ TEXT-TO-SPEECH
// =================================================================
bool speakGroq(const String& originalText) {
  if (originalText.length() == 0) {
      return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
  }
  String text = originalText;
  // Orpheus input limit protection
  if (text.length() > 190) {
      text = text.substring(0, 190);
      text += ".";
  }
  // =============================================================
  // HTTPS SETUP
  // =============================================================
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = "https://api.groq.com/openai/v1/audio/speech";
  if (!https.begin(client, url)) {
      Serial.println( "[TTS] HTTPS begin failed" );
      return false;
  }
  https.useHTTP10(true);
  https.setReuse(false);
  https.addHeader( "Content-Type", "application/json" );
  https.addHeader( "Authorization", "Bearer " + String(GROQ_API_KEY) );
  https.setTimeout(60000);
  // =============================================================
  // CREATE JSON REQUEST
  // =============================================================
  DynamicJsonDocument request(2048);
  request["model"] = GROQ_TTS_MODEL;
  request["input"] = text;
  request["voice"] = GROQ_TTS_VOICE;
  request["response_format"] = "wav";
  request["sample_rate"] = SPEAKER_SAMPLE_RATE;
  String body;
  serializeJson( request, body );
  Serial.println( "[TTS] Generating speech..." );
  // =============================================================
  // SEND REQUEST
  // =============================================================
  int httpCode = https.POST(body);
  Serial.print( "[TTS HTTP] " );
  Serial.println( httpCode );
  if ( httpCode < 200 || httpCode >= 300 ) {
      String errorResponse = https.getString();
      Serial.println();
      Serial.println( "===== GROQ TTS ERROR =====" );
      Serial.println( errorResponse );
      Serial.println( "==========================" );
      https.end();
      return false;
  }
  // =============================================================
  // CONTENT LENGTH
  // =============================================================
  int contentLength = https.getSize();
  Serial.print( "[TTS] Content-Length: " );
  Serial.println( contentLength );
  if (contentLength < 0) {
      Serial.println( "[TTS] Chunked/unknown length response" );
  }
  // =============================================================
  // ALLOCATE PSRAM BUFFER
  // =============================================================
  // 2 MB is enough for our short voice responses.
  const size_t MAX_TTS_WAV = 2 * 1024 * 1024;
  uint8_t* wav = (uint8_t*)ps_malloc( MAX_TTS_WAV );
  if (wav == nullptr) {
      Serial.println( "[TTS] PSRAM allocation failed" );
      https.end();
      return false;
  }
  // =============================================================
  // READ AUDIO STREAM
  // =============================================================
  WiFiClient* stream = https.getStreamPtr();
  size_t received = 0;
  unsigned long lastDataTime = millis();
  Serial.println( "[TTS] Downloading WAV..." );
  while ( https.connected() || stream->available() ) {
      size_t available = stream->available();
      if (available > 0) {
          size_t remaining = MAX_TTS_WAV - received;
          if (remaining == 0) {
              Serial.println( "[TTS] WAV buffer full!" );
              break;
          }
          size_t amount = available;
          if (amount > 4096) {
              amount = 4096;
          }
          if (amount > remaining) {
              amount = remaining;
          }
          int count = stream->readBytes( wav + received, amount );
          if (count > 0) {
              received += count;
              lastDataTime = millis();
          }
      }
      else
      {
          // No data for 10 seconds -> stop
          if ( millis() - lastDataTime > 10000 ) {
              Serial.println( "[TTS] Stream timeout" );
              break;
          }
          delay(1);
      }
  }
  https.end();
  // =============================================================
  // CHECK DOWNLOAD
  // =============================================================
  Serial.print( "[TTS] Received WAV bytes: " );
  Serial.println( received );
  if (received < 44) {
      Serial.println( "[TTS] WAV download failed" );
      free(wav);
      return false;
  }
  // =============================================================
  // VERIFY WAV
  // =============================================================
  if ( memcmp( wav, "RIFF", 4 ) != 0 ) {
      Serial.println( "[TTS] Invalid RIFF header" );
      Serial.print( "[TTS] First bytes: " );
      for ( int i = 0; i < 12 && i < received; i++ ) {
          Serial.printf( "%02X ", wav[i] );
      }
      Serial.println();
      free(wav);
      return false;
  }
  if ( memcmp( wav + 8, "WAVE", 4 ) != 0 ) {
      Serial.println( "[TTS] Invalid WAVE header" );
      free(wav);
      return false;
  }
  Serial.println( "[TTS] Valid WAV received" );
  // =============================================================
  // PLAY AUDIO
  // =============================================================
  bool success = playWavFromMemory( wav, received );
  // =============================================================
  // FREE MEMORY
  // =============================================================
  free(wav);
  Serial.print( "[TTS] Free PSRAM after playback: " );
  Serial.println( ESP.getFreePsram() );
  return success;
}
// =================================================================
// PLAY WAV FROM PSRAM
// =================================================================
bool playWavFromMemory( uint8_t* wav, size_t wavSize ) {
  if ( wav == nullptr || wavSize < 44 ) {
      return false;
  }
  // -------------------------------------------------------------
  // Verify RIFF/WAVE
  // -------------------------------------------------------------
  if ( memcmp( wav, "RIFF", 4 ) != 0 ) {
      Serial.println( "[SPEAKER] RIFF header missing" );
      return false;
  }
  if ( memcmp( wav + 8, "WAVE", 4 ) != 0 ) {
      Serial.println( "[SPEAKER] WAVE header missing" );
      return false;
  }
  // -------------------------------------------------------------
  // Search WAV chunks
  // -------------------------------------------------------------
  size_t offset = 12;
  uint16_t audioFormat = 0;
  uint16_t channels = 0;
  uint16_t bitsPerSample = 0;
  uint32_t sampleRate = 0;
  uint8_t* pcmData = nullptr;
  size_t pcmSize = 0;
  while ( offset + 8 <= wavSize ) {
      char chunkID[5];
      memcpy( chunkID, wav + offset, 4 );
      chunkID[4] = '\0';
      uint32_t chunkSize = readLE32( wav, offset + 4 );
      size_t dataOffset = offset + 8;
      if ( dataOffset > wavSize ) {
          break;
      }
      if ( chunkSize > wavSize - dataOffset ) {
          chunkSize = wavSize - dataOffset;
      }
      if ( memcmp( chunkID, "fmt ", 4 ) == 0 ) {
          if ( chunkSize >= 16 ) {
              audioFormat = readLE16( wav, dataOffset );
              channels = readLE16( wav, dataOffset + 2 );
              sampleRate = readLE32( wav, dataOffset + 4 );
              bitsPerSample = readLE16( wav, dataOffset + 14 );
          }
      }
      if ( memcmp( chunkID, "data", 4 ) == 0 ) {
          pcmData = wav + dataOffset;
          pcmSize = chunkSize;
          break;
      }
      // WAV chunks are word aligned
      offset = dataOffset + chunkSize + (chunkSize & 1);
  }
  if ( pcmData == nullptr || pcmSize == 0 ) {
      Serial.println( "[SPEAKER] WAV data chunk not found" );
      return false;
  }
  Serial.print( "[SPEAKER] Format: " );
  Serial.println( audioFormat );
  Serial.print( "[SPEAKER] Channels: " );
  Serial.println( channels );
  Serial.print( "[SPEAKER] Sample rate: " );
  Serial.println( sampleRate );
  Serial.print( "[SPEAKER] Bits: " );
  Serial.println( bitsPerSample );
  Serial.print( "[SPEAKER] PCM bytes: " );
  Serial.println( pcmSize );
  // -------------------------------------------------------------
  // We expect PCM16 mono 24 kHz
  // -------------------------------------------------------------
  if ( audioFormat != 1 ) {
      Serial.println( "[SPEAKER] Unsupported WAV encoding" );
      return false;
  }
  if ( bitsPerSample != 16 ) {
      Serial.println( "[SPEAKER] Expected 16-bit PCM" );
      return false;
  }
  if ( sampleRate != SPEAKER_SAMPLE_RATE ) {
      Serial.print( "[SPEAKER] Unexpected sample rate: " );
      Serial.println( sampleRate );
      return false;
  }
  // -------------------------------------------------------------
  // Mono playback
  // -------------------------------------------------------------
  if ( channels == 1 ) {
      Serial.println( "[SPEAKER] Speaking..." );
      size_t position = 0;
      while ( position < pcmSize ) {
          size_t chunk = pcmSize - position;
          if ( chunk > 4096 ) {
              chunk = 4096;
          }
          size_t bytesWritten = 0;
          esp_err_t result = i2s_channel_write( speakerTX, pcmData + position, chunk, &bytesWritten, 1000 );
          if ( result != ESP_OK ) {
              Serial.print( "[SPEAKER] I2S error: " );
              Serial.println( result );
              return false;
          }
          position += bytesWritten;
      }
      Serial.println( "[SPEAKER] Finished" );
      return true;
  }
  // -------------------------------------------------------------
  // Stereo -> mono conversion
  // -------------------------------------------------------------
  if ( channels == 2 ) {
      Serial.println( "[SPEAKER] Stereo WAV -> mono..." );
      const int FRAMES = 512;
      int16_t mono[FRAMES];
      int16_t* stereo = (int16_t*)pcmData;
      size_t totalFrames = pcmSize / 4;
      size_t framePosition = 0;
      Serial.println( "[SPEAKER] Speaking..." );
      while ( framePosition < totalFrames ) {
          size_t frames = totalFrames - framePosition;
          if ( frames > FRAMES ) {
              frames = FRAMES;
          }
          for ( size_t i = 0; i < frames; i++ ) {
              int32_t left = stereo[ (framePosition + i) * 2 ];
              int32_t right = stereo[ (framePosition + i) * 2 + 1 ];
              mono[i] = (int16_t)( (left + right) / 2 );
          }
          size_t bytesWritten = 0;
          esp_err_t result = i2s_channel_write( speakerTX, mono, frames * sizeof(int16_t), &bytesWritten, 1000 );
          if ( result != ESP_OK ) {
              Serial.println( "[SPEAKER] I2S write failed" );
              return false;
          }
          framePosition += frames;
      }
      Serial.println( "[SPEAKER] Finished" );
      return true;
  }
  Serial.println( "[SPEAKER] Unsupported channel count" );
  return false;
}
// =================================================================
// PLAY LOCAL WAV FROM LITTLEFS
// =================================================================
bool playLocalWav(const char* filename) {
  Serial.println();
  Serial.print("[LOCAL WAV] Opening: ");
  Serial.println(filename);
  File file = LittleFS.open(filename, "r");
  if (!file) {
      Serial.println("[ERROR] Local WAV file not found");
      return false;
  }
  size_t fileSize = file.size();
  Serial.print("[LOCAL WAV] File size: ");
  Serial.println(fileSize);
  if (fileSize <= 44) {
      Serial.println("[ERROR] Invalid WAV file");
      file.close();
      return false;
  }
  // -------------------------------------------------------------
  // Read standard 44-byte WAV header
  // -------------------------------------------------------------
  uint8_t header[44];
  if (file.read(header, 44) != 44) {
      Serial.println("[ERROR] Could not read WAV header");
      file.close();
      return false;
  }
  // Verify RIFF
  if (memcmp(header, "RIFF", 4) != 0) {
      Serial.println("[ERROR] RIFF header missing");
      file.close();
      return false;
  }
  // Verify WAVE
  if (memcmp(header + 8, "WAVE", 4) != 0) {
      Serial.println("[ERROR] WAVE header missing");
      file.close();
      return false;
  }
  uint16_t audioFormat = readLE16(header, 20);
  uint16_t channels = readLE16(header, 22);
  uint32_t sampleRate = readLE32(header, 24);
  uint16_t bitsPerSample = readLE16(header, 34);
  Serial.print("[LOCAL WAV] Sample rate: ");
  Serial.println(sampleRate);
  Serial.print("[LOCAL WAV] Channels: ");
  Serial.println(channels);
  Serial.print("[LOCAL WAV] Bits: ");
  Serial.println(bitsPerSample);
  // -------------------------------------------------------------
  // Check our required audio format
  // -------------------------------------------------------------
  if ( audioFormat != 1 || channels != 1 || sampleRate != SPEAKER_SAMPLE_RATE || bitsPerSample != 16 ) {
      Serial.println( "[ERROR] WAV must be PCM16, mono, 24000 Hz" );
      file.close();
      return false;
  }
  // -------------------------------------------------------------
  // Stream PCM data to MAX98357A
  // -------------------------------------------------------------
  uint8_t buffer[1024];
  Serial.println("[LOCAL WAV] Playing...");
  while (file.available()) {
      size_t bytesRead = file.read(buffer, sizeof(buffer));
      if (bytesRead == 0) {
          break;
      }
      size_t bytesWritten = 0;
      esp_err_t result = i2s_channel_write( speakerTX, buffer, bytesRead, &bytesWritten, portMAX_DELAY );
      if (result != ESP_OK) {
          Serial.println("[ERROR] I2S write failed");
          file.close();
          return false;
      }
  }
  file.close();
  Serial.println("[LOCAL WAV] Playback finished");
  return true;
}

// =================================================================
// PLAY LOCAL MP3 FROM LITTLEFS
// =================================================================
void playLocalMP3(const char* filename) {
    Serial.print("[MP3] Playing: ");
    Serial.println(filename);
    audio.connecttoFS(LittleFS, filename);
}



// =================================================================
// INTENT MATCHER - Maps question → MP3 file
// =================================================================
String matchIntent(String question) {
    question.toLowerCase();
    question.trim();

    // ---------- LOCATION & CAMPUS ----------
    if (question.indexOf("where") >= 0 || question.indexOf("location") >= 0)
        return "/bbd_location.mp3";
    if (question.indexOf("full name") >= 0 || question.indexOf("full form") >= 0)
        return "/bbd_full_name.mp3";
    if (question.indexOf("former") >= 0 || question.indexOf("old name") >= 0)
        return "/bbd_former_name.mp3";
    if (question.indexOf("aktu") >= 0 || question.indexOf("college code") >= 0)
        return "/bbd_aktu_code.mp3";
    if (question.indexOf("address") >= 0)
        return "/bbd_campus_address.mp3";
    if (question.indexOf("facilities") >= 0)
        return "/bbd_campus_facilities.mp3";
    if (question.indexOf("hostel available") >= 0 || question.indexOf("hostel facility") >= 0)
        return "/bbd_hostel_available.mp3";
    if (question.indexOf("sports") >= 0 || question.indexOf("stadium") >= 0)
        return "/bbd_sports_stadium.mp3";
    if (question.indexOf("library") >= 0)
        return "/bbd_library.mp3";
    if (question.indexOf("bank") >= 0 || question.indexOf("atm") >= 0)
        return "/bbd_bank.mp3";
    if (question.indexOf("gym") >= 0 || question.indexOf("gymnasium") >= 0)
        return "/bbd_gym.mp3";
    if (question.indexOf("cafeteria") >= 0 || question.indexOf("food court") >= 0)
        return "/bbd_cafeteria.mp3";
    if (question.indexOf("wifi") >= 0 || question.indexOf("wi-fi") >= 0)
        return "/bbd_wifi.mp3";
    if (question.indexOf("medical") >= 0 || question.indexOf("dispensary") >= 0)
        return "/bbd_medical.mp3";
    if (question.indexOf("cctv") >= 0 || question.indexOf("security") >= 0)
        return "/bbd_cctv.mp3";

    // ---------- COURSES & FEES ----------
    if (question.indexOf("courses") >= 0)
        return "/bbd_courses.mp3";
    if (question.indexOf("btech specialization") >= 0 || question.indexOf("branches") >= 0)
        return "/bbd_btech_specializations.mp3";
    if (question.indexOf("btech fee") >= 0 || question.indexOf("total fee") >= 0)
        return "/bbd_btech_fee.mp3";
    if (question.indexOf("annual fee") >= 0 || question.indexOf("per year") >= 0)
        return "/bbd_btech_annual_fee.mp3";
    if (question.indexOf("eligibility") >= 0 || question.indexOf("criteria") >= 0)
        return "/bbd_btech_eligibility.mp3";
    if (question.indexOf("mba") >= 0)
        return "/bbd_mba.mp3";
    if (question.indexOf("mtech fee") >= 0)
        return "/bbd_mtech_fee.mp3";
    if (question.indexOf("mtech eligibility") >= 0)
        return "/bbd_mtech_eligibility.mp3";
    if (question.indexOf("mba eligibility") >= 0)
        return "/bbd_mba_eligibility.mp3";
    if (question.indexOf("mba specialization") >= 0)
        return "/bbd_mba_specializations.mp3";
    if (question.indexOf("management quota") >= 0)
        return "/bbd_management_quota.mp3";
    if (question.indexOf("btech admission") >= 0)
        return "/bbd_btech_admission_process.mp3";
    if (question.indexOf("mba admission") >= 0)
        return "/bbd_mba_admission_process.mp3";
    if (question.indexOf("mca") >= 0)
        return "/bbd_mca.mp3";
    if (question.indexOf("btech duration") >= 0)
        return "/bbd_btech_duration.mp3";
    if (question.indexOf("mba duration") >= 0)
        return "/bbd_mba_duration.mp3";
    if (question.indexOf("mtech duration") >= 0)
        return "/bbd_mtech_duration.mp3";
    if (question.indexOf("ece fee") >= 0)
        return "/bbd_btech_ece_fee.mp3";
    if (question.indexOf("ai ml") >= 0 || question.indexOf("aiml") >= 0)
        return "/bbd_btech_aiml_fee.mp3";
    if (question.indexOf("scholarship") >= 0)
        return "/bbd_scholarship.mp3";

    // ---------- PLACEMENTS ----------
    if (question.indexOf("highest package") >= 0)
        return "/bbd_highest_package.mp3";
    if (question.indexOf("average package") >= 0)
        return "/bbd_average_package.mp3";
    if (question.indexOf("recruiters") >= 0 || question.indexOf("companies") >= 0)
        return "/bbd_top_recruiters.mp3";
    if (question.indexOf("placement percentage") >= 0 || question.indexOf("placement rate") >= 0)
        return "/bbd_placement_percentage.mp3";
    if (question.indexOf("tcs") >= 0)
        return "/bbd_tcs_priority.mp3";
    if (question.indexOf("placement start") >= 0)
        return "/bbd_placement_start.mp3";
    if (question.indexOf("placement process") >= 0)
        return "/bbd_placement_process.mp3";
    if (question.indexOf("internship") >= 0)
        return "/bbd_internships.mp3";
    if (question.indexOf("cse average") >= 0)
        return "/bbd_cse_average_package.mp3";
    if (question.indexOf("cse highest") >= 0)
        return "/bbd_cse_highest_package.mp3";
    if (question.indexOf("google") >= 0 || question.indexOf("samsung") >= 0)
        return "/bbd_google_visit.mp3";
    if (question.indexOf("ece placement") >= 0)
        return "/bbd_ece_placement.mp3";
    if (question.indexOf("it placement") >= 0)
        return "/bbd_it_placement.mp3";
    if (question.indexOf("placement cell") >= 0)
        return "/bbd_placement_cell.mp3";
    if (question.indexOf("mba average") >= 0)
        return "/bbd_mba_average_package.mp3";
    if (question.indexOf("mba highest") >= 0)
        return "/bbd_mba_highest_package.mp3";
    if (question.indexOf("deloitte") >= 0)
        return "/bbd_deloitte_visit.mp3";
    if (question.indexOf("cognizant") >= 0)
        return "/bbd_cognizant_visit.mp3";
    if (question.indexOf("tech mahindra") >= 0)
        return "/bbd_tech_mahindra_visit.mp3";
    if (question.indexOf("hcl") >= 0)
        return "/bbd_hcl_visit.mp3";

    // ---------- HOSTEL LIFE ----------
    if (question.indexOf("hostel fee") >= 0)
        return "/bbd_hostel_fee.mp3";
    if (question.indexOf("hostel room") >= 0)
        return "/bbd_hostel_rooms.mp3";
    if (question.indexOf("hostel wifi") >= 0)
        return "/bbd_hostel_wifi.mp3";
    if (question.indexOf("mess") >= 0)
        return "/bbd_mess_fee.mp3";
    if (question.indexOf("hostel security") >= 0)
        return "/bbd_hostel_security.mp3";
    if (question.indexOf("girls ac") >= 0 || question.indexOf("2 seater ac") >= 0)
        return "/bbd_hostel_fee_girls_2ac.mp3";
    if (question.indexOf("boys non ac") >= 0 || question.indexOf("3 seater non ac") >= 0)
        return "/bbd_hostel_fee_boys_3nonac.mp3";
    if (question.indexOf("boys 2 seater") >= 0)
        return "/bbd_hostel_fee_boys_2nonac.mp3";
    if (question.indexOf("boys 3 seater ac") >= 0)
        return "/bbd_hostel_fee_boys_3ac.mp3";
    if (question.indexOf("registration fee") >= 0)
        return "/bbd_hostel_registration_fee.mp3";
    if (question.indexOf("girls hostel") >= 0)
        return "/bbd_girls_hostel.mp3";
    if (question.indexOf("boys hostel") >= 0)
        return "/bbd_boys_hostel.mp3";
    if (question.indexOf("boys capacity") >= 0)
        return "/bbd_boys_hostel_capacity.mp3";
    if (question.indexOf("girls capacity") >= 0)
        return "/bbd_girls_hostel_capacity.mp3";
    if (question.indexOf("hostel tv") >= 0)
        return "/bbd_hostel_tv.mp3";
    if (question.indexOf("common room") >= 0)
        return "/bbd_hostel_common_room.mp3";
    if (question.indexOf("hostel rules") >= 0)
        return "/bbd_hostel_rules.mp3";
    if (question.indexOf("cleanliness") >= 0 || question.indexOf("hygiene") >= 0)
        return "/bbd_hostel_cleanliness.mp3";
    if (question.indexOf("hostel food") >= 0)
        return "/bbd_hostel_food.mp3";
    if (question.indexOf("hostel sports") >= 0)
        return "/bbd_hostel_sports.mp3";

    // ---------- ADMISSION & OTHER ----------
    if (question.indexOf("apply online") >= 0 || question.indexOf("application") >= 0)
        return "/bbd_apply_online.mp3";
    if (question.indexOf("entrance") >= 0 || question.indexOf("jee") >= 0 || question.indexOf("cat") >= 0)
        return "/bbd_entrance_exams.mp3";
    if (question.indexOf("scholarships available") >= 0)
        return "/bbd_scholarships_available.mp3";
    if (question.indexOf("website") >= 0 || question.indexOf("official site") >= 0)
        return "/bbd_official_website.mp3";
    if (question.indexOf("email") >= 0 || question.indexOf("contact") >= 0)
        return "/bbd_contact_email.mp3";
    if (question.indexOf("phone") >= 0 || question.indexOf("number") >= 0)
        return "/bbd_contact_phone.mp3";
    if (question.indexOf("student mall") >= 0 || question.indexOf("shop") >= 0)
        return "/bbd_student_mall.mp3";
    if (question.indexOf("salon") >= 0)
        return "/bbd_salon.mp3";
    if (question.indexOf("clothing") >= 0)
        return "/bbd_clothing_store.mp3";
    if (question.indexOf("coffee") >= 0)
        return "/bbd_coffee_shop.mp3";
    if (question.indexOf("fest") >= 0 || question.indexOf("utkarsh") >= 0)
        return "/bbd_annual_fest.mp3";
    if (question.indexOf("february") >= 0 || question.indexOf("utkarsh month") >= 0)
        return "/bbd_utkarsh_month.mp3";
    if (question.indexOf("utkarsh events") >= 0)
        return "/bbd_utkarsh_events.mp3";
    if (question.indexOf("tech fest") >= 0 || question.indexOf("tech expo") >= 0)
        return "/bbd_tech_fest.mp3";
    if (question.indexOf("director") >= 0)
        return "/bbd_director.mp3";
    if (question.indexOf("hod cse") >= 0 || question.indexOf("head cse") >= 0)
        return "/bbd_hod_cse.mp3";
    if (question.indexOf("professor cse") >= 0)
        return "/bbd_professor_cse.mp3";
    if (question.indexOf("hod civil") >= 0)
        return "/bbd_hod_civil.mp3";
    if (question.indexOf("hod ece") >= 0)
        return "/bbd_hod_ece.mp3";
    if (question.indexOf("hod electrical") >= 0)
        return "/bbd_hod_electrical.mp3";
    if (question.indexOf("hod mechanical") >= 0)
        return "/bbd_hod_mechanical.mp3";
    if (question.indexOf("alumni") >= 0 || question.indexOf("shweta") >= 0)
        return "/bbd_alumni_1.mp3";
    if (question.indexOf("deepanshu") >= 0 || question.indexOf("arora") >= 0)
        return "/bbd_alumni_2.mp3";
    if (question.indexOf("vision") >= 0)
        return "/bbd_vision.mp3";

    // ---------- NO MATCH ----------
    return "";
}

// =================================================================
// JSON ESCAPE HELPER
// =================================================================
String jsonEscape( const String& input ) {
  String output = "";
  output.reserve( input.length() + 32 );
  for ( size_t i = 0; i < input.length(); i++ ) {
      char c = input[i];
      switch (c) {
          case '\\\\': output += "\\\\\\\";
              break;
          case '"': output += "\\\\\"";
              break;
          case '\n': output += "\\\n";
              break;
          case '\r': output += "\\\r";
              break;
          case '\t': output += "\\\t";
              break;
          default: output += c;
              break;
      }
  }
  return output;
}

