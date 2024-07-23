// TODO



// Shortcuts
function addEvent(e, ev, f) { e.addEventListener(ev, f, false) }
function el(e) { return document.getElementById(e) }

// Technically not a header but a good indication of a proper z64 file
const Z64_MAGIC = '\x80\x37\x12\x40';

var romFile;

addEvent(window, 'load', function () {
    el('input-file-rom').value = '';

    addEvent(el('input-file-rom'), 'change', function () {
        setBuildEnabled(false);
        romFile = new MarcFile(this, parseROM);
    });

    addEvent(el('button-build'), 'click', function () {
        el('infotext-right').innerHTML = "Working...";
        generateAndSaveUF2(romFile, el('checkbox-compress').checked);
    });

});


function setBuildEnabled(status) {
    setElementEnabled('input-file-rom', status);
    if (romFile && status) {
        setElementEnabled('button-build', status);
    } else {
        setElementEnabled('button-build', false);
    }
}


function parseROM() {
    if (!romFile.readString(4).startsWith(Z64_MAGIC)) {
        el('infotext-right').innerHTML = "Might not be a z64 file!";
    } else {
        el('infotext-right').innerHTML = "";
    }

    updateChecksums(romFile, 0);
}


function updateChecksums(file, startOffset, force) {
    el('crc32').innerHTML = 'Calculating...';
    el('md5').innerHTML = 'Calculating...';

    el('crc32').innerHTML = padZeroes(crc32(file, startOffset), 4);
    el('md5').innerHTML = padZeroes(md5(file, startOffset), 16);

    if (window.crypto && window.crypto.subtle && window.crypto.subtle.digest) {
        el('sha1').innerHTML = 'Calculating...';
        sha1(file);
    }

    setBuildEnabled(true);
}


function setElementEnabled(element, status) {
    if (status) {
        el(element).className = 'enabled';
    } else {
        el(element).className = 'disabled';
    }
    el(element).disabled = !status;
}

function stringToUint8Array(str) {
    return new Uint8Array(str.split('').map(function(char) {return char.charCodeAt(0);}));
}


function compileUF2(headerAndMappingU8, payload) {
    let offset = 0;

    // Calculate the output size
    var blocks = Math.ceil(payload.fileSize / 256 + 0x8000 / 256);
    var outSize = blocks * 512;
    var dataOut = new Uint8Array(outSize);

    var block = {
        flags: UF2Flags.familyIDPresent,
        flashAddress: 0x10030000,
        payload: null,
        blockNumber: 0,
        totalBlocks: blocks,
        boardFamily: familyMap['Raspberry Pi RP2040'],
    };

    // Write header
    i = 0;
    while (i < headerAndMappingU8.length) {
        len = Math.min(headerAndMappingU8.length - i, 256)
        block.payload = headerAndMappingU8.slice(i, i + len);
        encodeBlock(block, dataOut, offset);
        block.blockNumber++;
        block.flashAddress += 256;
        offset += 512;
        i += len
    }

    // Write payload
    payload.seek(0);
    i = 0;
    while (i < payload.fileSize) {
        len = Math.min(payload.fileSize - i, 256)
        block.payload = payload.readBytes(len);
        encodeBlock(block, dataOut, offset);
        block.blockNumber++;
        block.flashAddress += 256;
        offset += 512;
        i += len
    }

    return dataOut;
}

// Compares two Uint8Array
function Uint8ArrayHasPrefix(buf, prefix) {
    if (buf.byteLength < prefix.byteLength) {
        return false;
    }

    for (var i = 0; i < prefix.byteLength; i++) {
        if (buf[i] != prefix[i]) {
            return false;
        }
    }

    return true;
}

// Returns the index of the Uint8Array needle in the array of Uint8Arrays haystack
function indexOfUint8Array(needle, haystack) {
    for (var i = 0; i < haystack.length; i++) {
        if (Uint8ArrayHasPrefix(haystack[i], needle)) {
            return i;
        }
    }

    return -1;
}

function generateAndSaveUF2(rom, compress) {
    var uf2data;
    var blob;

    if (compress) {
        var headerAndMapping = new ArrayBuffer(0x8000);
        var headerAndMappingU8 = new Uint8Array(headerAndMapping, 0);
        headerAndMappingU8.set(stringToUint8Array("picocartcompress"));

        var mapping = new DataView(headerAndMapping, 16);

        chunk_size_pot = 10
        chunk_size = 1 << chunk_size_pot

        // compressedROM

        // Split rom into chunk_size chunks
        unique_chunks = []
        chunk_mapping = []
        chunk_idx = 0
        num_chunks = Math.ceil(rom.fileSize / chunk_size)
        rom.seek(0)
        for (var i = 0; i < num_chunks; i++) {
            len = Math.min(rom.fileSize - i*chunk_size, chunk_size)
            chunk_data = rom.slice(i * chunk_size, len)._u8array;

            // Check if chunk_data is in unique_chunks
            var index = indexOfUint8Array(chunk_data, unique_chunks);
            if (index == -1) {
                // Found a unique chunk
                unique_chunks.push(chunk_data)
                index = chunk_idx
                chunk_idx += 1
            }

            chunk_mapping.push(index)
            // console.log(`${i} -> ${index}`)
        }

        // Flatten chunk_mapping into a Uint8Array
        for (var i = 0; i < chunk_mapping.length; i++) {
            mapping.setUint16(i*2, chunk_mapping[i], true); // little-endian
        }

        // Flatten unique_chunks into a Uint8Array
        ucn = unique_chunks.length
        var compressedROM = new Uint8Array(chunk_size * (ucn - 1) + unique_chunks[ucn - 1].byteLength);
        for (var i = 0; i < ucn; i++) {
            compressedROM.set(unique_chunks[i], chunk_size * i);
        }

        romSize = rom.fileSize;
        newSize = headerAndMapping.byteLength + compressedROM.byteLength;
        ratio = newSize / romSize;
        lastAddr = 0x10030000 + newSize;
        flashEnd = 0x10000000 + 16*1024*1024;
        console.log(`Full ROM size:   ${romSize} bytes`)
        console.log(`Header + Chunks: ${newSize} bytes`)
        console.log(`Ratio:           ${ratio * 100} %`)
        console.log(`Address of last byte: 0x${lastAddr.toString(16)} / 0x${flashEnd.toString(16)}`)

        el('infotext-left').innerHTML = "Required flash:";
        el('infotext-right').innerHTML = `${((30000 + newSize) / 1024/1024).toFixed(2)} MB`;
        if (lastAddr > flashEnd) {
            alert("Flash overflow, the compressed ROM will not fit.")
        }

        uf2data = compileUF2(headerAndMappingU8, new MarcFile(compressedROM));
    } else {
        var headerAndMapping = new ArrayBuffer(0x8000);
        var headerAndMappingU8 = new Uint8Array(headerAndMapping, 0);
        headerAndMappingU8.set(stringToUint8Array("picocart        "));
        uf2data = compileUF2(headerAndMappingU8, rom);
    }

    // Convert dataOut to a blob and save it with saveAs
    try {
        blob = new Blob([uf2data], { type: "uf2" });
    } catch (e) {
        //old browser, use BlobBuilder
        window.BlobBuilder = window.BlobBuilder || window.WebKitBlobBuilder || window.MozBlobBuilder || window.MSBlobBuilder;
        if (e.name === 'InvalidStateError' && window.BlobBuilder) {
            var bb = new BlobBuilder();
            bb.append(uf2data.buffer);
            blob = bb.getBlob("uf2");
        } else {
            throw new Error('Incompatible Browser');
            return false;
        }
    }

    const uf2FileName = rom.fileName.substr(0, rom.fileName.lastIndexOf(".")) + ".uf2";

    saveAs(blob, uf2FileName);
}






/* FileSaver.js (source: http://purl.eligrey.com/github/FileSaver.js/blob/master/src/FileSaver.js)
 * A saveAs() FileSaver implementation.
 * 1.3.8
 * 2018-03-22 14:03:47
 *
 * By Eli Grey, https://eligrey.com
 * License: MIT
 *   See https://github.com/eligrey/FileSaver.js/blob/master/LICENSE.md
 */
var saveAs = saveAs || function (c) { "use strict"; if (!(void 0 === c || "undefined" != typeof navigator && /MSIE [1-9]\./.test(navigator.userAgent))) { var t = c.document, f = function () { return c.URL || c.webkitURL || c }, s = t.createElementNS("http://www.w3.org/1999/xhtml", "a"), d = "download" in s, u = /constructor/i.test(c.HTMLElement) || c.safari, l = /CriOS\/[\d]+/.test(navigator.userAgent), p = c.setImmediate || c.setTimeout, v = function (t) { p(function () { throw t }, 0) }, w = function (t) { setTimeout(function () { "string" == typeof t ? f().revokeObjectURL(t) : t.remove() }, 4e4) }, m = function (t) { return /^\s*(?:text\/\S*|application\/xml|\S*\/\S*\+xml)\s*;.*charset\s*=\s*utf-8/i.test(t.type) ? new Blob([String.fromCharCode(65279), t], { type: t.type }) : t }, r = function (t, n, e) { e || (t = m(t)); var r, o = this, a = "application/octet-stream" === t.type, i = function () { !function (t, e, n) { for (var r = (e = [].concat(e)).length; r--;) { var o = t["on" + e[r]]; if ("function" == typeof o) try { o.call(t, n || t) } catch (t) { v(t) } } }(o, "writestart progress write writeend".split(" ")) }; if (o.readyState = o.INIT, d) return r = f().createObjectURL(t), void p(function () { var t, e; s.href = r, s.download = n, t = s, e = new MouseEvent("click"), t.dispatchEvent(e), i(), w(r), o.readyState = o.DONE }, 0); !function () { if ((l || a && u) && c.FileReader) { var e = new FileReader; return e.onloadend = function () { var t = l ? e.result : e.result.replace(/^data:[^;]*;/, "data:attachment/file;"); c.open(t, "_blank") || (c.location.href = t), t = void 0, o.readyState = o.DONE, i() }, e.readAsDataURL(t), o.readyState = o.INIT } r || (r = f().createObjectURL(t)), a ? c.location.href = r : c.open(r, "_blank") || (c.location.href = r); o.readyState = o.DONE, i(), w(r) }() }, e = r.prototype; return "undefined" != typeof navigator && navigator.msSaveOrOpenBlob ? function (t, e, n) { return e = e || t.name || "download", n || (t = m(t)), navigator.msSaveOrOpenBlob(t, e) } : (e.abort = function () { }, e.readyState = e.INIT = 0, e.WRITING = 1, e.DONE = 2, e.error = e.onwritestart = e.onprogress = e.onwrite = e.onabort = e.onerror = e.onwriteend = null, function (t, e, n) { return new r(t, e || t.name || "download", n) }) } }("undefined" != typeof self && self || "undefined" != typeof window && window || this);