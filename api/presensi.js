// Database sementara di memori server untuk keperluan testing
let registeredCards = {
  "A1B2C3D4": "Budi Santoso",
  "E5F60718": "Siti Aminah",
  "C3CF6C12": "Fathul"
};

let attendanceLogs = [];

export default function handler(req, res) {
  // Mengizinkan CORS
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    return res.status(200).end();
  }

  // GET: Mengambil data log & kartu terdaftar
  if (req.method === 'GET') {
    return res.status(200).json({
      cards: registeredCards,
      logs: attendanceLogs
    });
  }

  // PUT: Menambah kartu baru dari React UI
  if (req.method === 'PUT') {
    const { uid, nama } = req.body || {};
    if (!uid || !nama) {
      return res.status(400).json({ message: "UID dan Nama wajib diisi" });
    }
    const cleanUid = uid.trim().toUpperCase();
    registeredCards[cleanUid] = nama.trim();
    return res.status(200).json({ message: "Kartu ditambahkan", cards: registeredCards });
  }

  // POST: Endpoint utama yang dipanggil oleh ESP8266
  if (req.method === 'POST') {
    const { uid_kartu } = req.body || {};

    if (!uid_kartu) {
      return res.status(400).json({
        status: "error",
        nama: "Invalid Payload"
      });
    }

    const uid = uid_kartu.trim().toUpperCase();
    const namaUser = registeredCards[uid];
    const timestamp = new Date().toLocaleString('id-ID', { timeZone: 'Asia/Makassar' });

    if (namaUser) {
      attendanceLogs.unshift({
        id: Date.now(),
        uid,
        nama: namaUser,
        status: "success",
        waktu: timestamp
      });

      console.log(`[tap] ${uid} -> success: ${namaUser}`);
      return res.status(200).json({
        status: "success",
        nama: namaUser.substring(0, 16)
      });
    } else {
      attendanceLogs.unshift({
        id: Date.now(),
        uid,
        nama: "Tidak Dikenali",
        status: "error",
        waktu: timestamp
      });

      console.log(`[tap] ${uid} -> error: unknown card`);
      return res.status(200).json({
        status: "error",
        nama: "Unknown"
      });
    }
  }

  return res.status(405).json({ message: "Method Not Allowed" });
}
