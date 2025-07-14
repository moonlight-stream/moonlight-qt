# Artemis Protocol Analysis

This document analyzes the Artemis Android implementation to understand the protocols used for enhanced features.

## 📋 Clipboard Sync Protocol

### HTTP Endpoints
- **Base URL**: `https://<server>/actions/clipboard`
- **Get Clipboard**: `GET /actions/clipboard?type=text`
- **Set Clipboard**: `POST /actions/clipboard?type=text` with text content as body

### Implementation Details (from NvHTTP.java)
```java
public String getClipboard() throws IOException {
    return openHttpConnectionToString(httpClientLongConnectTimeout, 
        getHttpsUrl(true), "actions/clipboard", "type=text");
}

public Boolean sendClipboard(String content) throws IOException {
    String resp = openHttpConnectionToString(httpClientLongConnectTimeout, 
        getHttpsUrl(true), "actions/clipboard", "type=text", 
        RequestBody.create(content, MediaType.parse("text/plain")));
    // For handling the 200ed 404 from Sunshine
    return resp.isEmpty();
}
```

### Smart Clipboard Sync (from Game.java)
- **Automatic Upload**: On stream start/resume
- **Automatic Download**: When client loses focus
- **Loop Prevention**: Uses `CLIPBOARD_IDENTIFIER` to mark synced content
- **Size Limits**: Configurable maximum clipboard size
- **Toast Notifications**: Optional success/failure notifications

### Settings
- `smartClipboardSync`: Enable/disable automatic sync
- `smartClipboardSyncToast`: Show toast notifications
- `hideClipboardContent`: Mark remote clipboard as sensitive

## 🔐 OTP Pairing Protocol

### Standard Pairing vs OTP Pairing
**Standard PIN Pairing:**
```
phrase=getservercert&salt=<salt>&clientcert=<cert>
```

**OTP Pairing (with passphrase):**
```
phrase=getservercert&salt=<salt>&clientcert=<cert>&otpauth=<hash>
```

### OTP Hash Generation (from PairingManager.java)
```java
MessageDigest digest = MessageDigest.getInstance("SHA-256");
String plainText = pin + saltStr + passphrase;
byte[] hash = digest.digest(plainText.getBytes());
String hexString = bytesToHex(hash);
// Add to pairing arguments: &otpauth=<hexString>
```

### UI Flow
1. User selects "OTP Pair" from menu
2. Client shows 4-digit PIN input dialog
3. User enters PIN they provided to Apollo server
4. Client generates SHA-256 hash and sends pairing request
5. Apollo server validates the OTP and completes pairing

### Requirements
- Only works with Apollo server (not standard GameStream)
- User must manually enter PIN on Apollo web UI first
- PIN is limited to 4 digits in Android implementation

## ⚡ Server Commands Protocol

### Permission System (from ComputerDetails.java)
```java
enum Operations {
    clipboard_set    = 1 << 0,  // Allow set clipboard from client
    clipboard_read   = 1 << 1,  // Allow read clipboard from host
    file_upload      = 1 << 2,  // Allow file upload
    file_dwnload     = 1 << 3,  // Allow file download
    server_cmd       = 1 << 4,  // Allow execute server cmd
}
```

### UI Implementation (from GameMenu.java)
- Menu option: "Server Commands"
- Checks for `server_cmd` permission before showing
- Shows error dialog if no commands available or permission denied
- Error message references Apollo server requirement

### Error Handling
```java
builder.setTitle(R.string.game_dialog_title_server_cmd_empty);
builder.setMessage(R.string.game_dialog_message_server_cmd_empty);
// Message: "Please make sure you're using Apollo as server software 
//          and has been granted this client with 'Server Command' permission."
```

## 🔍 Key Insights for Qt Implementation

### 1. HTTP-Based Protocol
All Artemis features use standard HTTP/HTTPS requests to the Apollo server. No custom protocols or complex networking required.

### 2. Apollo Server Dependency
- Clipboard sync requires Apollo server with `/actions/clipboard` endpoint
- OTP pairing only works with Apollo (not standard GameStream)
- Server commands require Apollo with permission system

### 3. Permission-Based Features
- Client must check server capabilities before enabling features
- Apollo server controls which clients can use which features
- Graceful degradation when features not available

### 4. Simple Data Formats
- Clipboard: Plain text only (`text/plain`)
- OTP: Simple SHA-256 hash
- Commands: Standard HTTP requests (implementation details TBD)

## 🛠️ Qt Implementation Plan

### Phase 1: HTTP Foundation
1. Extend existing `NvHTTP` class with Artemis endpoints
2. Add clipboard GET/POST methods
3. Implement permission checking system

### Phase 2: Clipboard Sync
1. Create `ClipboardManager` with Qt clipboard integration
2. Implement smart sync logic (auto upload/download)
3. Add loop prevention and size limits
4. Create settings UI

### Phase 3: OTP Pairing
1. Extend `PairingManager` with OTP support
2. Add SHA-256 hash generation
3. Create OTP input dialog
4. Integrate with existing pairing flow

### Phase 4: Server Commands
1. Research Apollo server command API
2. Implement command execution framework
3. Create command management UI
4. Add permission-based feature enabling

## 📝 Missing Information

### Server Commands Details
- **Command Format**: How are commands defined and sent?
- **Response Handling**: What does the server return?
- **Command Discovery**: How does client get available commands?
- **Parameters**: Do commands support parameters?

### Apollo Server API
- **Documentation**: Need Apollo server API documentation
- **Endpoints**: What other endpoints are available?
- **Authentication**: How are permissions managed?
- **Versioning**: How to detect Apollo vs standard GameStream?

## 🔗 References

### Source Files Analyzed
- `app/src/main/java/com/limelight/Game.java` (lines 1880-2030)
- `app/src/main/java/com/limelight/nvstream/http/NvHTTP.java` (lines 920-940)
- `app/src/main/java/com/limelight/nvstream/http/PairingManager.java` (lines 200-250)
- `app/src/main/java/com/limelight/GameMenu.java` (server commands)
- `app/src/main/java/com/limelight/nvstream/http/ComputerDetails.java` (permissions)

### Key Constants
- `CLIPBOARD_IDENTIFIER`: Used to prevent sync loops
- `OTP_CODE_LENGTH`: 4 digits for PIN input
- `actions/clipboard`: HTTP endpoint for clipboard operations
- `otpauth`: Parameter for OTP pairing authentication