const net = require('net');
const fs = require('fs');
const path = require('path');

const intMaxValue = 4294967295;
let filename = "C:\\Users\\Tahir\\AppData\\Local\\arduino\\sketches\\56E72E47308BAABB32C268F6AC23F42C\\km0_km4_image2.bin";
let host = "192.168.29.23";
let port = 8082;

const loadIP = () => {
    const ipFilePath = path.join(__dirname, "ip.txt");
    if (!fs.existsSync(ipFilePath)) {
        fs.writeFileSync(ipFilePath, host);
    }
    let ip = process.argv.slice(2)[0];
    if (ip) {
        fs.writeFileSync(ipFilePath, ip); 
    } else {
        ip = fs.readFileSync(path.join(__dirname, "ip.txt")).toString();
    }
    return ip;
}

const loadFilePath = () => {
    const otaFilePath = path.join(__dirname, "ota.txt");
    if (!fs.existsSync(otaFilePath)) {
        fs.writeFileSync(otaFilePath, filename);
    }
    let ota = process.argv.slice(2)[1];
    if (ota) {
        fs.writeFileSync(otaFilePath, ota); 
    } else {
        ota = fs.readFileSync(path.join(__dirname, "ota.txt")).toString();
    }
    return ota;
}



function calculateChecksum(filePath) {
    return new Promise((resolve, reject) => {
        let checksum = 0;
        const stream = fs.createReadStream(filePath);

        stream.on('data', (chunk) => {
            for (let i = 0; i < chunk.length; i++) {
                checksum = (checksum + chunk[i]) % intMaxValue;
            }
        });

        stream.on('end', () => {
            resolve(checksum);
        });

        stream.on('error', (err) => {
            reject(err);
        });
    });
}

async function sendFile() {
    try {
        filename = loadFilePath();
        host = host//loadIP();
        console.log(filename, host)
        const length = fs.statSync(filename).size;
        const checksum = await calculateChecksum(filename);
        const empty = 0;
        let progress = 0;
        console.log({
            "binary-file": filename,
            ip: host,
            Checksum: checksum.toString(16),
            Length: length
        });

        // Prepare header
        const headerBuffer = Buffer.alloc(12);
        headerBuffer.writeUInt32LE(checksum, 0);
        headerBuffer.writeUInt32LE(empty, 4);
        headerBuffer.writeUInt32LE(length, 8);

        // Connect and send
        const client = new net.Socket();
        client.connect(port, host, () => {
            console.log('Connected to server');
            client.write(headerBuffer);

            const fileStream = fs.createReadStream(filename);
            fileStream.pipe(client, { end: false });
            fileStream.on('data', (data) => {
                progress += data.length;
                console.log(Math.round(progress * 100 / length) + '%');
            })
            fileStream.on('end', () => {
                console.log("File sent successfully");
                client.end();
            });
        });

        client.on('close', () => {
            console.log('Connection closed');
        });

        client.on('error', (err) => {
            console.error("Socket error:", err);
        });

    } catch (err) {
        return error;
    }
}
(async () => {
    try {
        await sendFile();
    } catch(error) {
        console.error(error);
    }
})()