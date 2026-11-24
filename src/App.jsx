import { useEffect, useState } from 'react'
import './App.css'

const API = import.meta.env.VITE_API_URL || '/api/presensi'

const MENU = [
  { id: 'alat', label: 'Alat' },
  { id: 'kartu', label: 'Kartu RFID' },
  { id: 'presensi', label: 'Presensi' },
]

function App() {
  const [menu, setMenu] = useState('alat')
  const [data, setData] = useState({ cards: {}, logs: [], lastRead: null })
  const [terhubung, setTerhubung] = useState(false)

  useEffect(() => {
    let hidup = true
    const ambil = async () => {
      try {
        const res = await fetch(API)
        if (!res.ok) throw new Error('bad response')
        const json = await res.json()
        if (hidup) {
          setData(json)
          setTerhubung(true)
        }
      } catch {
        if (hidup) setTerhubung(false)
      }
    }
    ambil()
    const id = setInterval(ambil, 2000)
    return () => {
      hidup = false
      clearInterval(id)
    }
  }, [])

  return (
    <div className="layout">
      <aside className="sidebar">
        <h1>RFID Presensi</h1>
        <nav>
          {MENU.map((m) => (
            <button
              key={m.id}
              className={menu === m.id ? 'aktif' : ''}
              onClick={() => setMenu(m.id)}
            >
              {m.label}
            </button>
          ))}
        </nav>
        <div className={`status ${terhubung ? 'ok' : ''}`}>
          {terhubung ? 'Server terhubung' : 'Server terputus'}
        </div>
      </aside>

      <main className="konten">
        {menu === 'alat' && <MenuAlat lastRead={data.lastRead} terhubung={terhubung} />}
        {menu === 'kartu' && <MenuKartu cards={data.cards} />}
        {menu === 'presensi' && <MenuPresensi logs={data.logs} />}
      </main>
    </div>
  )
}

function MenuAlat({ lastRead, terhubung }) {
  return (
    <>
      <h2>Detail Alat</h2>
      <p className="sub">
        Mode dibanti lewat tombol di device — lihat mode aktif di LCD. Scan di
        mode Baca akan tampil di sini.
      </p>

      <div className="kartu">
        <div className="label">UID terakhir (mode baca)</div>
        {lastRead ? (
          <>
            <div className="uid-besar">{lastRead.uid}</div>
            <p className="sub" style={{ margin: '8px 0 0' }}>
              {lastRead.waktu}
            </p>
          </>
        ) : (
          <p className="kosong">Belum ada scan mode baca</p>
        )}
      </div>

      <div className="grid-dua">
        <div className="kartu">
          <div className="label">Status server</div>
          <div>{terhubung ? 'Terhubung' : 'Terputus'}</div>
        </div>
        <div className="kartu">
          <div className="label">Endpoint</div>
          <div style={{ fontSize: 13, wordBreak: 'break-all' }}>{API}</div>
        </div>
      </div>
    </>
  )
}

function MenuKartu({ cards }) {
  const [uid, setUid] = useState('')
  const [nama, setNama] = useState('')
  const [pesan, setPesan] = useState('')
  const daftar = Object.entries(cards)

  const tambah = async (e) => {
    e.preventDefault()
    setPesan('')
    try {
      const res = await fetch(API, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ uid, nama }),
      })
      const json = await res.json()
      if (!res.ok) throw new Error(json.message || 'Gagal')
      setPesan(`Kartu ${uid.trim().toUpperCase()} ditambahkan`)
      setUid('')
      setNama('')
    } catch (err) {
      setPesan(`Error: ${err.message}`)
    }
  }

  return (
    <>
      <h2>Kartu RFID</h2>
      <p className="sub">Daftar kartu terdaftar untuk presensi.</p>

      <div className="kartu">
        <div className="label">Tambah kartu</div>
        <form className="tambah" onSubmit={tambah}>
          <input
            placeholder="UID (hex, contoh A1B2C3D4)"
            value={uid}
            onChange={(e) => setUid(e.target.value)}
            required
          />
          <input
            placeholder="Nama pemilik"
            value={nama}
            onChange={(e) => setNama(e.target.value)}
            required
          />
          <button type="submit">Tambah</button>
        </form>
        {pesan && <p className="pesan">{pesan}</p>}
      </div>

      <div className="kartu">
        <div className="label">Kartu terdaftar ({daftar.length})</div>
        {daftar.length === 0 ? (
          <p className="kosong">Belum ada kartu</p>
        ) : (
          <table>
            <thead>
              <tr>
                <th>UID</th>
                <th>Nama</th>
              </tr>
            </thead>
            <tbody>
              {daftar.map(([u, n]) => (
                <tr key={u}>
                  <td style={{ fontFamily: 'ui-monospace, monospace' }}>{u}</td>
                  <td>{n}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </>
  )
}

function MenuPresensi({ logs }) {
  return (
    <>
      <h2>Presensi</h2>
      <p className="sub">Log absensi terbaru, disimpan di memori server (reset saat cold start).</p>

      <div className="kartu">
        <div className="label">Log ({logs.length})</div>
        {logs.length === 0 ? (
          <p className="kosong">Belum ada presensi</p>
        ) : (
          <table>
            <thead>
              <tr>
                <th>Waktu</th>
                <th>UID</th>
                <th>Nama</th>
                <th>Status</th>
              </tr>
            </thead>
            <tbody>
              {logs.map((log) => (
                <tr key={log.id}>
                  <td>{log.waktu}</td>
                  <td style={{ fontFamily: 'ui-monospace, monospace' }}>{log.uid}</td>
                  <td>{log.nama}</td>
                  <td>
                    <span className={`badge ${log.status === 'success' ? 'ok' : 'err'}`}>
                      {log.status === 'success' ? 'Hadir' : 'Ditolak'}
                    </span>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </>
  )
}

export default App
