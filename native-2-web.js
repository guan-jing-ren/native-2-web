var number_size = {
    Boolean: 1,
    Int8Array: 1,
    Uint8Array: 1,
    Int16Array: 2,
    Uint16Array: 2,
    Int32Array: 4,
    Uint32Array: 4,
    Float32Array: 4,
    Float64Array: 8,
}

var pad = number_size[Float64Array];

function calc_padding(size, count) {
    return pad == 0 ? 0 : (pad - ((count * size) % pad)) % pad;
}

function read_number(data, offset, type) {
    let n = type.prototype.constructor.call(data, offset, number_size[type])[0];
    offset += calc_padding(number_size[type], size);
    return [n, offset];
}

function read_numbers_bounded(data, offset, type, size) {
    let numbers = type.prototype.constructor.call(data, offset, size * number_size[type]);
    offset += calc_padding(number_size[type], size);
    return [numbers, offset];
}

function read_numbers(data, offset, type) {
    let size;
    [size, offset] = read_number(data, offset, Uint32Array);
    return read_numbers_bounded(data, offset, type, size);
}

function read_string(data, offset) {
    let s;
    [s, offset] = read_numbers(data, offset, Uint8Array);
    return [s, offset];
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
    [size, offset] = read_number(data, offset, Uint32Array);
    return read_structures_bounded(data, offset, readers, size);
}

function read_associative_bounded(data, offset, key_reader, value_reader, size) {
    let keys, values, map = {};
    [keys, offset] = key_reader(data, offset, size);
    [values, offset] = value_reader(data, offset, size);
    for (let k in keys) map[keys[k]] = values[k];
    return map;
}

function read_associative(data, offset, key_reader, value_reader) {
    let size;
    [size, offset] = read_number(data, offset, Uint32Array);
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
        subdivide(source, subdest, sublevel);
        dest.push(subdest);
    }
}

function read_multiarray(data, offset, type, extents) {
    let total = extents.reduce((p, c) => { return c + p; }, 0);
    let array, dest = [];
    if (number_size[type] !== undefined)
        [array, offset] = read_numbers_bounded(data, offset, type, total);
    else
        [array, offset] = read_structures_bounded(data, offset, type, total);
    subdivide(array, dest, extents);
    return [dest, offset];
}