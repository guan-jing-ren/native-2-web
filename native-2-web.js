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

  if (base_names && base_names.length > 0) object.__bases = bases;
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

function create_gatherer() {
  return d3.dispatch('gather');
}

function dispatch(dispatcher, value) {
  dispatcher.on('gather', value);
}

function subdispatch(dispatcher, subdispatchers, value) {
  dispatcher.on('gather', () => {
    subdispatchers.forEach(s => s.call('gather'));
    value();
  });
}

function persona_terminal(persona, parent) {
  d3.select(parent).classed(parent, true);
}

function persona_bool(persona, parent) {
  let node = d3.select(parent)
                 .append('input')
                 .classed(persona, true)
                 .attr('type', 'checkbox')
                 .attr('value', false)
                 .node();
  return () => node.checked ? true : false;
}

function persona_number(persona, parent) {
  let node = d3.select(parent)
                 .append('input')
                 .classed(persona, true)
                 .attr('type', 'number')
                 .node();
  return () => node.value || 0;
}

function persona_enum_option(persona, parent, value, name) {
  return d3.select(parent)
      .append('option')
      .classed(persona, true)
      .attr('value', value)
      .text(name)
      .node();
}

function persona_enum(persona, parent, enums) {
  let select = d3.select(parent).append('select').classed(persona, true);
  return [() => +select.node().value, select.node()];
}

function persona_char(persona, parent) {
  let node = d3.select(parent)
                 .append('input')
                 .classed(persona, true)
                 .attr('type', 'text')
                 .node();
  return () => node.value[0] || '\0';
}

function persona_string(persona, parent) {
  let node = d3.select(parent)
                 .append('input')
                 .classed(persona, true)
                 .attr('type', 'text')
                 .node();
  return () => node.value || '';
}

function persona_structure_baselabel(persona, parent) {
  return d3.select(parent)
      .append('td')
      .classed(persona, true)
      .text('__bases:')
      .node();
}

function persona_structure_bases(persona, parent) {
  return d3.select(parent)
      .append('td')
      .classed(persona, true)
      .append('table')
      .node();
}

function persona_structure_baseholder(persona, parent) {
  return d3.select(parent).append('tr').classed(persona, true).node();
}

function persona_structure(persona, parent) {
  return d3.select(parent).append('table').classed(persona, true).node();
}

function persona_structure_base(persona, parent, base_name) {
  let row = d3.select(parent).append('tr').classed(persona, true);
  row.append('td').text(base_name);
  return row.append('td').attr('class', 'n2w-html').node();
}

function persona_structure_memlabel(persona, parent, member_name) {
  return d3.select(parent)
      .append('td')
      .classed(persona, true)
      .text(member_name + ': ')
      .node();
}

function persona_structure_memvalue(persona, parent) {
  return d3.select(parent)
      .append('td')
      .classed(persona, true)
      .attr('class', 'n2w-html')
      .node();
}

function persona_structure_memholder(persona, parent) {
  return d3.select(parent).append('tr').classed(persona, true).node();
}

function persona_container(persona, parent) {
  return d3.select(parent).append('table').classed(persona, true).node();
}

function persona_container_element(persona, parent) {
  return d3.select(parent)
      .append('tr')
      .classed(persona, true)
      .append('td')
      .classed('n2w-html', true)
      .node();
}

function persona_container_expander(persona, parent, on_expand) {
  d3.select(parent)
      .append('tr')
      .classed(persona, true)
      .append('td')
      .append('input')
      .attr('type', 'button')
      .attr('value', '+')
      .on('click', on_expand);
}

function persona_map_key(persona, parent) {
  return d3.select(parent)
      .classed('n2w-persona-container-element', false)
      .classed(persona, true)
      .node();
}

function persona_map_value(persona, parent) {
  d3.select(parent.parentElement).append('td').text('->');
  return d3.select(parent.parentElement)
      .append('td')
      .classed(persona, true)
      .classed('n2w-html', true)
      .node();
}

function html_bool(parent, value, dispatcher) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_bool || persona_bool)('n2w-persona-bool', parent);
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter()));
}

function html_number(parent, value, dispatcher) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_number || persona_number)('n2w-persona-number', parent);
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter()));
}

function html_enum(parent, value, dispatcher, enums) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_enum || persona_enum)('n2w-persona-enum', parent, enums);
  Object.keys(enums)
      .filter(k => enums[k].length > 0)
      .map(k => +k)
      .sort((l, r) => l - r)
      .forEach(
          k => (this.persona_enum_option || persona_enum_option)(
              'n2w-persona-enum-option', value_getter[1], k, enums[k]));
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter[0]()));
}

function html_char(parent, value, dispatcher) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_char || persona_char)('n2w-persona-char8', parent);
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter()));
}

function html_char32(parent, value, dispatcher) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_char || persona_char)('n2w-persona-char32', parent);
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter()));
}

function html_string(parent, value, dispatcher) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_string || persona_string)('n2w-persona-string', parent);
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter()));
}

function html_structure(
    parent, value, dispatcher, html, names, base_html, base_names) {
  let table = (this.persona_structure || persona_structure)(
      'n2w-persona-structure', parent);
  let subvalue = {};
  let subdispatchers = [];
  let basedispatchers = [];

  let bases = {};
  if (base_names && base_names.length > 0) {
    let base_holder =
        (this.persona_structure_baseholder || persona_structure_baseholder)(
            'n2w-persona-structure-baseholder', table);
    (this.persona_structure_baselabel || persona_structure_baselabel)(
        'n2w-persona-structure-baselabel', base_holder);
    let base_data = (this.persona_structure_bases || persona_structure_bases)(
        'n2w-persona-structure-bases', base_holder);
    base_html.forEach((h, i) => {
      let basedispatcher = (this.create_gatherer || create_gatherer)();
      basedispatchers.push(basedispatcher);
      h((this.persona_structure_base || persona_structure_base)(
            'n2w-persona-structure-base', base_data, base_names[i]),
        v => bases[base_names[i]] = v, basedispatcher);
    });
  }

  if (typeof(html) == "function") html = [html];

  if (Array.isArray(html))
    html.forEach((h, i) => {
      let subdispatcher = (this.create_gatherer || create_gatherer)();
      subdispatchers.push(subdispatcher);
      let mem_holder =
          (this.persona_structure_memholder || persona_structure_memholder)(
              'n2w-persona-structure-member-holder', table);
      (this.persona_structure_memlabel || persona_structure_memlabel)(
          'n2w-persona-structure-member-label', mem_holder,
          names ? names[i] : i);
      h((this.persona_structure_memvalue || persona_structure_memvalue)(
            'n2w-persona-structure-member', mem_holder),
        v => subvalue[names ? names[i] : i] = v, subdispatcher);
    });

  (this.subdispatch || subdispatch)(
      dispatcher, basedispatchers.concat(subdispatchers), () => {
        if (base_names && base_names.length > 0) subvalue.__bases = bases;
        value(subvalue);
      });
}

function html_container(parent, value, dispatcher, html, size) {
  let table = (this.persona_container || persona_container)(
      'n2w-persona-container', parent);
  let subvalue = [];
  let subdispatchers = [];

  if (size !== undefined)
    for (let i = 0; i < size; ++i) {
      let subdispatcher = (this.create_gatherer || create_gatherer)();
      subdispatchers.push(subdispatcher);
      html(
          (this.persona_container_element || persona_container_element)(
              'n2w-persona-container-element', table),
          v => subvalue[i] = v, subdispatcher);
    }
  else {
    let index = 0;
    (this.persona_container_expander || persona_container_expander)(
        'n2w-persona-container-expander', table, () => {
          let subdispatcher = (this.create_gatherer || create_gatherer)();
          subdispatchers.push(subdispatcher);
          let slot = index++;
          html(
              (this.persona_container_element || persona_container_element)(
                  'n2w-persona-container-element', table),
              v => { subvalue[slot] = v; }, subdispatcher);
        });
  }

  (this.subdispatch || subdispatch)(
      dispatcher, subdispatchers, () => value(subvalue));
}

var html_bounded = html_container;
var html_sequence = html_container;

function html_associative(parent, value, dispatcher, html_key, html_value) {
  let subvalue = {};
  let subdispatchers = [];

  (this.html_sequence || html_sequence)(
      parent, v => {}, (this.create_gatherer || create_gatherer)(),
      (p, v, d) => {
        let key_value = [];
        let key_subdispatcher = (this.create_gatherer || create_gatherer)(),
            value_subdispatcher = (this.create_gatherer || create_gatherer)();
        subdispatchers.push(key_subdispatcher);
        subdispatchers.push(value_subdispatcher);
        html_key(
            (this.persona_map_key || persona_map_key)('n2w-persona-map-key', p),
            v => {
              key_value[0] = v;
              if (key_value[1])
                subvalue[JSON.stringify(key_value[0])] = key_value[1];
            },
            key_subdispatcher);
        html_value(
            (this.persona_map_value || persona_map_value)(
                'n2w-persona-map-value', p),
            v => {
              key_value[1] = v;
              if (key_value[0])
                subvalue[JSON.stringify(key_value[0])] = key_value[1];
            },
            value_subdispatcher);
      });

  (this.subdispatch || subdispatch)(
      dispatcher, subdispatchers, () => value(subvalue));
}

/////////////////////////////////////////////////
// WebSocket management for native to web APIs //
/////////////////////////////////////////////////

function create_service(pointer, writer, reader) {
  return function(ws) {
    let listener = function(e) {
      ws.removeEventListener('message', listener);
      let ret = reader(new DataView(e.data), 0);
      if (ret)
        this.callback(ret[0]);
      else
        this.callback();
    }.bind(this);
    ws.binaryType = "arraybuffer";

    let args = [...arguments];
    args.shift();

    this.then = function(handler) {
      this.callback = handler;
      ws.addEventListener('message', listener);
      ws.send(pointer);
      args = writer(args) || new ArrayBuffer();
      ws.send(args);
    }.bind(this);

    return this;
  };
}
function create_push_notifier(pointer, writer, reader) {}
function create_kaonashi(pointer, writer) {
  return function(ws) {
    ws.send(pointer);
    ws.send(writer(...arguments));
  };
}