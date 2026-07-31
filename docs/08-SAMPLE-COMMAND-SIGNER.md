# Sample: signing an admin command from Node-RED

Drop this in a **Function** node ahead of the MQTT-out node for any
protected topic (`access/cmd/enroll`, `access/cmd/enroll/save`,
`access/cmd/card/edit`, `access/cmd/card/delete`, `access/cmd/door`,
`access/cmd/ota/enable`, `access/cmd/factory_reset`,
`access/cmd/provision/open`). Requires Node.js's built-in `crypto` module
(already available inside Node-RED Function nodes).

```js
// Node-RED Function node
// Set `msg.topic` to the target command topic before this node, and put
// your command's fields (uid, name, cmd, etc.) in msg.payload.
// Set NEX_CMD_SECRET below to the device's cmdSecret (retrieved once from
// the device during provisioning — see docs/03-SECURITY-AUDIT.md).

const crypto = require('crypto');
const NEX_CMD_SECRET = "PASTE_DEVICE_CMD_SECRET_HERE";

const ts = Math.floor(Date.now() / 1000);
const nonce = crypto.randomBytes(12).toString('hex');

// Build the body WITHOUT sig, in the exact key order you want signed —
// object key order is preserved by JSON.stringify for string keys.
const body = { ...msg.payload, ts, nonce };
const canonicalBody = JSON.stringify(body);
const canonical = `${msg.topic}|${ts}|${nonce}|${canonicalBody}`;

const sig = crypto.createHmac('sha256', NEX_CMD_SECRET)
                   .update(canonical)
                   .digest('hex');

msg.payload = { ...body, sig };
return msg;
```

## Example: triggering an OTA update

```js
msg.topic = "access/cmd/ota/enable";
msg.payload = {
    cmd: "START_HTTP_OTA",
    version: "2.2.0",
    url: "https://backend.example.com/firmware/2.2.0/firmware.bin",
    sha256: "<64-char hex sha256 of firmware.bin>",
    signature: "",   // optional — see security/ota_security.h
    force: false
};
// ...then the signing code above, unchanged.
```

**Important:** the firmware computes its own canonical string by taking the
JSON it received, removing `"sig"`, and re-serializing with
`ArduinoJson::serializeJson()` — which preserves the field order it parsed
in. That means the object you sign must have `sig` added **last**, after
`ts`/`nonce`/your other fields, exactly as the snippet above does
(`{ ...body, sig }`). If you reorder fields after signing, verification
will fail.
