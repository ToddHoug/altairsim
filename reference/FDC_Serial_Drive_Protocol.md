# FDC+ Serial Drive Communications Protocol

Source: [protocol.txt](#)

> Protocol version 1.0, 3/05/15, M. Douglas. The wire protocol between the FDC+ (in serial
> drive mode) and a PC drive server that holds the disk images. All transactions are started
> by the FDC; the server only answers. See also [FDC+ Manual](FDC%2B%20Manual.md).

## Serial link

8N1, one of three rates, in order of preference:

| Rate | Notes |
|---|---|
| **403.2K** | Preferred. Full disk speed, and the most accurate rate the FDC can generate. |
| 460.8K | Full speed, but the FDC rate is off by about 3.5% — works, borderline. |
| 230.4K | On almost every PC port, within 2% of the FDC rate, but runs at 80–90% of real disk speed. |

## Message format (both directions)

Every command and response is a fixed **10-byte** message. Words are **16-bit little endian**.

| Bytes | 0–3 | 4–5 | 6–7 | 8–9 |
|---|---|---|---|---|
| FDC → server | Command (4 ASCII chars) | Parameter 1 | Parameter 2 | Checksum |
| Server → FDC | Response (4 ASCII chars) | Response Code | Response Data | Checksum |

**Checksum** = 16-bit sum of bytes 0–7.

## FDC → server commands

| Command | Parameter 1 | Parameter 2 | Meaning |
|---|---|---|---|
| `STAT` | LSB = selected drive number (`0xFF` = none selected); MSB = non-zero if head loaded | Current track number | Report FDC state, ask for mount status. |
| `READ` | Bits 15–12 = drive number; bits 11–0 = track number | Transfer length (must be the track length) | Read one whole track. |
| `WRIT` | Bits 15–12 = drive number; bits 11–0 = track number | Transfer length (must be the track length) | Write one whole track. |

- The FDC sends `STAT` about **ten times per second**, so head-load and track state stay current.
- A server may also treat every `READ` as "drive selected, head loaded, now on this track".
- A 4-bit drive field gives drives 0–15; a 12-bit track field gives tracks 0–4095.

## Server → FDC responses

| Response | Response Code | Response Data | Meaning |
|---|---|---|---|
| `STAT` | Ignored by the FDC | Mount bitmap: bit *n* = 1 if drive *n* has an image mounted (bits 15–0 = drives 15–0) | Answer to `STAT`. |
| `WRIT` | `OK` = ready for the track data; `Not Ready` if the request cannot be met (e.g. drive not mounted) | Don't care | Answer to `WRIT`, sent **before** the FDC sends data. |
| `WSTA` | Final write status (see codes) | Don't care | Sent after the server has received the track data. |

A `READ` has no separate response message: the server answers with the track data block.

### Response codes

| Code | Meaning |
|---|---|
| `0x0000` | OK |
| `0x0001` | Not Ready (e.g. write to an unmounted drive) |
| `0x0002` | Checksum error (e.g. on the block of write data) |
| `0x0003` | Write error (e.g. the write to the host disk failed) |

## Track data transfer

A track is sent as the data bytes followed by a **16-bit little-endian checksum** of the data
(16-bit sum). The transfer length in the command does **not** include the two checksum bytes.
The same format is used in both directions.

Sequences:

```
READ:  FDC  --READ-->            server
       FDC  <--track data+cksum--  server

WRIT:  FDC  --WRIT-->            server
       FDC  <--WRIT (OK/NotReady)-- server
       FDC  --track data+cksum-->   server      (only after OK)
       FDC  <--WSTA (status)--      server
```

## Error recovery

- **Timeout:** the FDC waits **one second** after the last byte of a message or data block;
  no answer in that time means the transmission was ignored.
- **Server, bad command checksum:** ignore the command. The FDC may retry it.
- **Server, bad write-data checksum:** do **not** ignore it — answer with Response Code
  `0x0002` (checksum error). *(The source says "the WRIT response"; since the data follows the
  `WRIT` response, this status is carried by `WSTA`.)* ⚠
- **FDC, bad response checksum:** the FDC ignores the response, and may retry by sending the
  command again.
