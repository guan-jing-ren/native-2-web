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
};

//////////////////////////////////
// Native to Javascript readers //
//////////////////////////////////

function read_number(data, offset, type) {
  let n = data[type](offset, true);
  offset += sizes[type];
  return [n, offset];
}

function read_enum(data, offset, type) {
  return read_number(data, offset, type);
}

function read_bool(data, offset) {
  let number = read_number(data, offset, 'getUint8');
  return [number[0] > 0 ? true : false, number[1]];
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
    c = (c << 6) |
        ((Array.isArray(data) ? data[offset + i] : data.getUint8(offset + i)) &
         0b00111111);
  return c;
}

function read_codepoint(data, offset) {
  let c = Array.isArray(data) ? data[offset] : data.getUint8(offset);
  if ((c & 0b11110000) == 0b11110000) {
    c = to_codepoint(c & 0b00000111, data, ++offset, 3);
    offset += 3;
  } else if ((c & 0b11100000) == 0b11100000) {
    c = to_codepoint(c & 0b00001111, data, ++offset, 2);
    offset += 2;
  } else if ((c & 0b11000000) == 0b11000000) {
    c = to_codepoint(c & 0b00011111, data, ++offset, 1);
    offset += 1;
  } else {  // if((c & 0b00000000) == 0b00000000) {
    offset += 1;
  }
  return [c, offset];
}

function read_char(data, offset) {
  return [String.fromCodePoint(data.getUint8(offset))[0], offset + 1];
}

function read_char32(data, offset) {
  return [
    String.fromCodePoint(data.getUint32(offset, true))[0],
    offset + Uint32Array.BYTES_PER_ELEMENT
  ];
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

function read_structure(
    data, offset, readers, names, base_readers, base_names) {
  let bases = {};
  if (base_names && base_names.length > 0)
    base_readers
        .map(r => {
          let o;
          [o, offset] = r(data, offset);
          return o;
        })
        .reduce((p, c, i) => {
          p[base_names[i]] = c;
          return p;
        }, bases);

  let object = {}, o;

  if (typeof readers === "function") readers = [readers];
  if (Array.isArray(readers))
    readers.forEach((v, i) => {
      [o, offset] = v(data, offset);
      object[names ? names[i] : i] = o;
    });

  object.__bases = bases;
  return [object, offset];
}

function read_structures_bounded(data, offset, reader, size) {
  let objects = [], o;
  for (let n = 0; n < size; ++n) {
    [o, offset] = reader(data, offset);
    objects.push(o);
  }
  return [objects, offset];
}

function read_structures(data, offset, reader) {
  let size;
  [size, offset] = read_number(data, offset, "getUint32");
  return read_structures_bounded(data, offset, reader, size);
}

function read_associative_bounded(
    data, offset, key_reader, value_reader, size) {
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
  let total = extents.reduce((p, c) => c + p, 0);
  let array, dest = [];
  if (sizes[type] !== undefined)
    [array, offset] = read_numbers_bounded(data, offset, type, total);
  else
    [array, offset] = read_structures_bounded(data, offset, type, total);
  subdivide(array, dest, extents);
  return [dest, offset];
}

//////////////////////////////////
// Javascript to native writers //
//////////////////////////////////

function concat_buffer(l, r) {
  if (l.length == 0) return r;
  if (r.length == 0) return l;
  [l, r] = [new DataView(l), new DataView(r)];
  let joined = new Uint8Array(l.buffer.byteLength + r.buffer.byteLength);
  let buf = new DataView(joined.buffer);
  for (let i = 0; i < l.byteLength; ++i) buf.setUint8(i, l.getUint8(i));
  for (let i = 0; i < r.byteLength; ++i)
    buf.setUint8(i + l.byteLength, r.getUint8(i));
  return buf.buffer;
}

function write_number(object, type) {
  let data = new DataView(new ArrayBuffer(sizes[type]));
  data[type](0, object, true);
  return data.buffer;
}

function write_enum(object, type) {
  return write_number(object, type);
}

function write_bool(object) {
  return write_number(object ? true : false, 'setUint8');
}

function write_numbers_bounded(object, type, size) {
  let data = new DataView(new ArrayBuffer(sizes[type] * size));
  for (let i = 0, offset = 0; i < size; ++i, offset += sizes[type])
    data[type](offset, object[i], true);
  return data.buffer;
}

function write_numbers(object, type) {
  let buffer = concat_buffer(
      write_number(object.length, "setUint32"),
      write_numbers_bounded(object, type, object.length));
  return buffer;
}

function from_codepoint(c) {
  if (c <= 0b01111111) return [c];
  if (c <= 0b011111111111)
    return [(c >> 6) | 0b11000000, (c & 0b111111) | 0b10000000];
  if (c <= 0b01111111111111111)
    return [
      (c >> 12) | 0b11100000, ((c >> 6) & 0b111111) | 0b10000000,
      (c & 0b111111) | 0b10000000
    ];
  if (c <= 0b0111111111111111111111)
    return [
      (c >> 18) | 0b11110000, ((c >> 12) & 0b111111) | 0b10000000,
      ((c >> 6) & 0b111111) | 0b10000000, (c & 0b111111) | 0b10000000
    ];
}

function write_char(object) {
  let buffer = Uint8Array.of(object.codePointAt(0)).buffer;
  return buffer;
}

function write_char32(object) {
  let buffer = Uint32Array.of(object.codePointAt(0)).buffer;
  return buffer;
}

function write_string(object) {
  let codepoints = [];
  for (let i = 0; i < object.length; ++i)
    codepoints.push(...from_codepoint(object.codePointAt(i)));
  let buffer = Uint8Array.of(...codepoints).buffer;
  buffer = concat_buffer(write_number(buffer.byteLength, "setUint32"), buffer);
  return buffer;
}

function write_structure(object, writers, names, base_writers, base_names) {
  let buffer;
  if (typeof writers === "function") writers = [writers];
  if (Array.isArray(writers))
    buffer = writers.map((v, i) => v(object[names ? names[i] : i]))
                 .reduce((p, c) => concat_buffer(p, c));
  if (base_names && base_names.length > 0) {
    let bases = base_writers.map((w, i) => w(object.__bases[base_names[i]]))
                    .reduce((p, c) => concat_buffer(p, c), []);
    buffer = concat_buffer(bases, buffer);
  }
  return buffer;
}

function write_structures_bounded(object, writer, size) {
  if (size == 0) return [];
  let o = [];
  for (let n = 0; n < size; ++n) o.push(writer(object[n]));
  let buffer = o.reduce((p, c) => concat_buffer(p, c));
  return buffer;
}

function write_structures(object, writer) {
  let buffer = concat_buffer(
      write_number(object.length, "setUint32"),
      write_structures_bounded(
          object, writer,
          Array.isArray(object) ? object.length : Object.keys(object).length));
  return buffer;
}

function write_associative_bounded(object, key_writer, value_writer, size) {
  let keys = Object.keys(object);
  let buffer = concat_buffer(
      key_writer(keys.map(JSON.parse), size),
      value_writer(keys.map(k => object[k]), size));
  return buffer;
}

function write_associative(object, key_writer, value_writer) {
  let buffer = concat_buffer(
      write_number(Object.keys(object).length, "setUint32"),
      write_associative_bounded(
          object, key_writer, value_writer, Object.keys(object).length));
  return buffer;
}

function subcombine(source, dest, extents) {
  if (extents.length == 1) {
    for (let i = 0, end = extents[0]; i < end; ++i) dest.push(source.shift());
    return;
  }

  let sublevel = [...extents];
  for (let i = 0, end = sublevel.shift(); i < end; ++i)
    subcombine(source[i], dest, [...sublevel]);
}

function write_multiarray(object, type, extents) {
  let dest = [];
  subcombine(object, dest, extents);
  let total = extents.reduce((p, c) => c + p, 0);
  let array;
  if (sizes[type] !== undefined)
    array = write_numbers_bounded(object, type, total);
  else
    array = write_structures_bounded(object, type, total);
  let buffer = concat_buffer(write_number(total, "setUint32"), array);
  return buffer;
}

//////////////////////////////
// Native to HTML generator //
//////////////////////////////

function html_bool(parent, value, dispatcher) {
  d3.select(parent).attr('class', null);
  let node = d3.select(parent)
                 .append('input')
                 .attr('type', 'checkbox')
                 .attr('value', false)
                 .node();
  dispatcher.on('gather', () => value(node.checked ? true : false));
}

function html_number(parent, value, dispatcher) {
  d3.select(parent).attr('class', null);
  let node = d3.select(parent).append('input').attr('type', 'number').node();
  dispatcher.on('gather', () => value(node.value || 0));
}

function html_enum(parent, value, dispatcher) {
  return html_number(parent, value, dispatcher);
}

function html_char(parent, value, dispatcher) {
  d3.select(parent).attr('class', null);
  let node = d3.select(parent).append('input').attr('type', 'text').node();
  dispatcher.on('gather', () => value(node.value[0] || '\0'));
}

function html_char32(parent, value, dispatcher) {
  d3.select(parent).attr('class', null);
  let node = d3.select(parent).append('input').attr('type', 'text').node();
  dispatcher.on('gather', () => value(node.value[0] || '\0'));
}

function html_string(parent, value, dispatcher) {
  d3.select(parent).attr('class', null);
  let node = d3.select(parent).append('input').attr('type', 'text').node();
  dispatcher.on('gather', () => value(node.value || ''));
}

function html_structure(
    parent, value, dispatcher, html, names, base_html, base_names) {
  let table = d3.select(parent).append('table').node();
  let subvalue = {};
  let subdispatchers = [];
  let basedispatchers = [];

  let bases = {};
  if (base_names && base_names.length > 0) {
    let base_table = d3.select(table).append('tr').append('td').append('table');
    let base_row = base_table.append('tr');
    base_row.append('td').text('__bases:');
    let base_data = base_row.append('td').append('table');
    base_html.forEach((h, i) => {
      let basedispatcher = d3.dispatch('gather');
      basedispatchers.push(basedispatcher);
      let row = base_data.append('tr');
      row.append('td').text(base_names[i]);
      h(row.append('td').attr('class', 'n2w-html').node(),
        v => bases[base_names[i]] = v, basedispatcher);
    });
  }

  if (typeof(html) == "function") html = [html];

  if (Array.isArray(html))
    html.forEach((h, i) => {
      let row = d3.select(table).append('tr');
      row.append('td').text((names ? names[i] : i) + ': ');
      let cell = row.append('td').attr('class', 'n2w-html').node();
      let subdispatcher = d3.dispatch('gather');
      subdispatchers.push(subdispatcher);
      h(cell, v => subvalue[names ? names[i] : i] = v, subdispatcher);
    });

  dispatcher.on('gather', () => {
    basedispatchers.forEach(b => b.call('gather'));
    subvalue.__bases = bases;
    subdispatchers.forEach(s => s.call('gather'));
    value(subvalue);
  });
}

function html_bounded(parent, value, dispatcher, html, size) {
  let table = d3.select(parent).append('table').node();
  let expand_row = d3.select(table).append('tr');
  let subvalue = [];
  let subdispatchers = [];

  for (let i = 0; i < size; ++i) {
    let subdispatcher = d3.dispatch('gather');
    subdispatchers.push(subdispatcher);
    html(
        expand_row.append('td').attr('class', 'n2w-html').node(),
        v => subvalue[i] = v, subdispatcher);
  }

  dispatcher.on('gather', () => {
    subdispatchers.forEach(s => s.call('gather'));
    value(subvalue);
  });
}

function html_sequence(parent, value, dispatcher, html) {
  let table = d3.select(parent).append('table').node();
  let expand_row = d3.select(table).append('tr');
  let subvalue = [], index = 0;
  let subdispatchers = [];

  let expand_button =
      expand_row.append('td')
          .append('input')
          .attr('type', 'button')
          .attr('value', '+')
          .on('click', () => {
            let cell = d3.select(table)
                           .append('tr')
                           .append('td')
                           .attr('class', 'n2w-html')
                           .node();
            let subdispatcher = d3.dispatch('gather');
            subdispatchers.push(subdispatcher);
            let slot = index++;
            html(cell, v => { subvalue[slot] = v; }, subdispatcher);
          });

  dispatcher.on('gather', () => {
    subdispatchers.forEach(s => s.call('gather'));
    value(subvalue);
  });
}

function html_associative(parent, value, dispatcher, html_key, html_value) {
  let subvalue = {};
  let subdispatchers = [];

  html_sequence(
      parent, v => {}, d3.dispatch('gather'),
      (p, v, d) => {
        let key_value = [];
        let key_subdispatcher = d3.dispatch('gather'),
            value_subdispatcher = d3.dispatch('gather');
        subdispatchers.push(key_subdispatcher);
        subdispatchers.push(value_subdispatcher);
        html_key(p, v => {
          key_value[0] = v;
          if (key_value[1])
            subvalue[JSON.stringify(key_value[0])] = key_value[1];
        }, key_subdispatcher);
        d3.select(p.parentElement).append('td').text('->');
        html_value(
            d3.select(p.parentElement)
                .append('td')
                .attr('class', 'n2w-html')
                .node(),
            v => {
              key_value[1] = v;
              if (key_value[0])
                subvalue[JSON.stringify(key_value[0])] = key_value[1];
            },
            value_subdispatcher);
      });

  dispatcher.on('gather', () => {
    subdispatchers.forEach(s => s.call('gather'));
    value(subvalue);
  });
}
