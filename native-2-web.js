var pad = Float64Array.BYTES_PER_ELEMENT;

function calc_padding(type, count) {
    return pad == 0 ? 0 : (pad - ((count * type.BYTES_PER_ELEMENT) % pad)) % pad;
}

function read_number(data, offset, type) {
    let n = new type(data, offset, 1);
    n = n[0];
    offset += pad;
    return [n, offset];
}

function read_numbers_bounded(data, offset, type, size) {
    let numbers = new type(data, offset, size);
    offset += size * type.BYTES_PER_ELEMENT + calc_padding(type, size);
    return [numbers, offset];
}

function read_numbers(data, offset, type) {
    let size;
    [size, offset] = read_number(data, offset, Uint32Array);
    return read_numbers_bounded(data, offset, type, size);
}

function read_string(data, offset, type) {
    let s;
    [s, offset] = read_numbers(data, offset, type);
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
    return [map, offset];
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