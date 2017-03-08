var sizes = {
    "getInt8": 1,
    "getInt16": 2,
    "getInt32": 4,
    "getUint8": 1,
    "getUint16": 2,
    "getUint32": 4,
    "getFloat32": 4,
    "getFloat64": 8,
    "setInt8": 1,
    "setInt16": 2,
    "setInt32": 4,
    "setUint8": 1,
    "setUint16": 2,
    "setUint32": 4,
    "setFloat32": 4,
    "setFloat64": 8,
}

function read_number(data, offset, type) {
    let n = data[type](offset, true);
    offset += sizes[type];
    return [n, offset];
}

function read_numbers_bounded(data, offset, type, size) {
    let numbers = [];
    for (let i = 0; i < size; ++i, offset += sizes[type])
        numbers.push(data[type](offset, true));
    return [numbers, offset];
}

function read_numbers(data, offset, type) {
    let size;
    [size, offset] = read_number(data, offset, "getUint32");
    return read_numbers_bounded(data, offset, type, size);
}

function to_codepoint(c, data, offset, count) {
    for (let i = 0; i < count; ++i)
        c = (c << 6) | ((Array.isArray(data) ? data[offset + i] : data.getUint8(offset + i)) & 0b00111111);
    return c;
}

function read_codepoint(data, offset) {
    let c = Array.isArray(data) ? data[offset] : data.getUint8(offset);
    if ((c & 0b11110000) == 0b11110000) {
        c = to_codepoint(c & 0b00000111, data, ++offset, 3);
        offset += 3;
    }
    else if ((c & 0b11100000) == 0b11100000) {
        c = to_codepoint(c & 0b00001111, data, ++offset, 2);
        offset += 2;
    }
    else if ((c & 0b11000000) == 0b11000000) {
        c = to_codepoint(c & 0b00011111, data, ++offset, 1);
        offset += 1;
    }
    else {// if((c & 0b00000000) == 0b00000000) {
        offset += 1;
    }
    return [c, offset];
}

function read_char(data, offset) {
    return [String.fromCodePoint(data.getUint8(offset))[0], offset + 1];
}

function read_char32(data, offset) {
    return [String.fromCodePoint(data.getUint32(offset, true))[0], offset + Uint32Array.BYTES_PER_ELEMENT];
}

function read_string(data, offset) {
    let s;
    [s, offset] = read_numbers(data, offset, "getUint8");
    let u = [];
    for (let i = 0; i < s.length;) {
        let c = 0;
        [c, i] = read_codepoint(s, i);
        u.push(c);
    }
    return [String.fromCodePoint(...u), offset];
}

function read_structure(data, offset, readers) {
    let object = [], o;
    if (typeof readers === "function")
        return readers(data, offset);

    if (Array.isArray(readers))
        for (let r in readers) {
            [o, offset] = readers[r](data, offset);
            object.push(o);
        }
    return [object, offset];
}

function read_structures_bounded(data, offset, readers, size) {
    let objects = [], o;
    for (let n = 0; n < size; ++n) {
        [o, offset] = read_structure(data, offset, readers);
        objects.push(o);
    }
    return [objects, offset];
}

function read_structures(data, offset, readers) {
    let size;
    [size, offset] = read_number(data, offset, "getUint32");
    return read_structures_bounded(data, offset, readers, size);
}

function read_associative_bounded(data, offset, key_reader, value_reader, size) {
    let keys, values, map = {};
    [keys, offset] = key_reader(data, offset, size);
    [values, offset] = value_reader(data, offset, size);
    for (let k in keys) map[JSON.stringify(keys[k])] = values[k];
    return [map, offset];
}

function read_associative(data, offset, key_reader, value_reader) {
    let size;
    [size, offset] = read_number(data, offset, "getUint32");
    return read_associative_bounded(data, offset, key_reader, value_reader, size);
}

function subdivide(source, dest, extents) {
    if (extents.length == 1) {
        for (let i = 0, end = extents[0]; i < end; ++i) {
            dest.push(source.shift());
        }
        return;
    }

    let sublevel = [...extents];
    for (let i = 0, end = sublevel.shift(); i < end; ++i) {
        let subdest = [];
        subdivide(source, subdest, [...sublevel]);
        dest.push(subdest);
    }
}

function read_multiarray(data, offset, type, extents) {
    let total = extents.reduce((p, c) => { return c + p; }, 0);
    let array, dest = [];
    if (sizes[type] !== undefined)
        [array, offset] = read_numbers_bounded(data, offset, type, total);
    else
        [array, offset] = read_structures_bounded(data, offset, type, total);
    subdivide(array, dest, extents);
    return [dest, offset];
}

function concat_buffer(l, r) {
    [l, r] = [new DataView(l), new DataView(r)];
    let joined = new Uint8Array(l.buffer.byteLength + r.buffer.byteLength);
    let buf = new DataView(joined.buffer);
    for (let i = 0; i < l.byteLength; ++i)
        buf.setUint8(i, l.getUint8(i));
    for (let i = 0; i < r.byteLength; ++i)
        buf.setUint8(i + l.byteLength, r.getUint8(i));
    return buf.buffer;
}

function write_number(object, type) {
    let data = new DataView(new ArrayBuffer(sizes[type]));
    data[type](0, object, true);
    return data.buffer;
}

function write_numbers_bounded(object, type, size) {
    let data = new DataView(new ArrayBuffer(sizes[type] * size));
    for (let i = 0, offset = 0; i < size; ++i, offset += sizes[type])
        data[type](offset, object[i], true);
    return data.buffer;

}

function write_numbers(object, type) {
    return concat_buffer(write_number(object.length, "setUint32"), write_numbers_bounded(object, type, object.length));
}

function from_codepoint(c) {
    if (c <= 0b01111111) return [c];
    if (c <= 0b011111111111) return [(c >> 6) | 0b11000000, (c & 0b111111) | 0b10000000];
    if (c <= 0b01111111111111111) return [(c >> 12) | 0b11100000, ((c >> 6) & 0b111111) | 0b10000000, (c & 0b111111) | 0b10000000];
    if (c <= 0b0111111111111111111111) return [(c >> 18) | 0b11110000, ((c >> 12) & 0b111111) | 0b10000000, ((c >> 6) & 0b111111) | 0b10000000, (c & 0b111111) | 0b10000000];
}

function write_char(object) {
    return Uint8Array.of(...from_codepoint(object)).buffer;
}

function write_char32(object) {
    return Uint32Array.of(object).buffer;
}

function write_string(object) {
    let codepoints = [];
    for (let i = 0; i < object.length; ++i)
        codepoints.push(...from_codepoint(object.codePointAt(i)));
    return concat_buffer(write_number(object.length, "setUint32"), Uint8Array.of(...codepoints).buffer);
}

function write_structure(object, writers) {
    if (typeof writers === "function")
        return writers(object);

    if (Array.isArray(writers))
        return writers.map(v => v(object)).reduce((p, c) => concat_buffer(p, c));
}

function write_structures_bounded(object, writers, size) {
    let o = [];
    for (let n = 0; n < size; ++n)
        o.push(write_structure(object, writers));
    return o.reduce((p, c) => concat_buffer(p, c));
}

function write_structures(object, writers) {
    return concat_buffer(write_number(object.length, "setUint32"), write_structures_bounded(object, writers, size));
}

function write_associative_bounded(object, key_writer, value_writer, size) {
    return concat_buffer(key_writer(object, size), value_writer(object, size));
}

function write_associative(object, key_writer, value_writer) {
    return concat_buffer(write_number(Object.keys(object).length, "setUint32"), write_associative_bounded(object, key_reader, value_reader, Object.keys(object).length));
}

function subcombine(source, dest, extents) {
    if (extents.length == 1) {
        for (let i = 0, end = extents[0]; i < end; ++i)
            dest.push(source.shift());
        return;
    }

    let sublevel = [...extents];
    for (let i = 0, end = sublevel.shift(); i < end; ++i)
        subcombine(source[i], dest, [...sublevel]);
}

function write_multiarray(object, type, extents) {
    let dest = [];
    subcombine(object, dest, extents);
    let total = extents.reduce((p, c) => { return c + p; }, 0);
    let array;
    if (sizes[type] !== undefined)
        array = write_numbers_bounded(object, type, total);
    else
        array = write_structures_bounded(object, type, total);
    return concat_buffer(write_number(total, "setUint32"), array);
}