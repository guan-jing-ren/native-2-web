var sizes = {
  'getInt8' : 1,
  'getInt16' : 2,
  'getInt32' : 4,
  'getUint8' : 1,
  'getUint16' : 2,
  'getUint32' : 4,
  'getFloat32' : 4,
  'getFloat64' : 8,
  'setInt8' : 1,
  'setInt16' : 2,
  'setInt32' : 4,
  'setUint8' : 1,
  'setUint16' : 2,
  'setUint32' : 4,
  'setFloat32' : 4,
  'setFloat64' : 8,
};

var __n2w_deleted_value = {};

//////////////////////////////////
// Native to Javascript readers //
//////////////////////////////////

function read_number(data, offset, type) {
  let n = data[type](offset, true);
  offset += sizes[type];
  return [ n, offset ];
  }

function read_enum(data, offset, type) {
  return read_number(data, offset, type);
  }

function read_bool(data, offset) {
  let number = read_number(data, offset, 'getUint8');
  return [ number[0] > 0 ? true : false, number[1] ];
  }

function read_numbers_bounded(data, offset, type, size) {
  let numbers = [];
  for (let i = 0; i < size; ++i, offset += sizes[type])
    numbers.push(data[type](offset, true));
  return [ numbers, offset ];
  }

function read_numbers(data, offset, type) {
  let size;
  [size, offset] = read_number(data, offset, 'getUint32');
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
  } else { // if((c & 0b00000000) == 0b00000000) {
    offset += 1;
    }
  return [ c, offset ];
  }

function read_char(data, offset) {
  return [ String.fromCodePoint(data.getUint8(offset))[0], offset + 1 ];
  }

function read_char32(data, offset) {
  return [
    String.fromCodePoint(data.getUint32(offset, true))[0],
    offset + Uint32Array.BYTES_PER_ELEMENT
  ];
  }

function read_string(data, offset) {
  let s;
  [s, offset] = read_numbers(data, offset, 'getUint8');
  let u = [];
  for (let i = 0; i < s.length;) {
    let c = 0;
    [c, i] = read_codepoint(s, i);
    u.push(c);
    }
  return [ String.fromCodePoint(...u), offset ];
  }

function read_structure(data, offset, readers, names, base_readers,
                        base_names) {
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

  if (typeof readers === 'function')
    readers = [ readers ];
  if (Array.isArray(readers))
    readers.forEach((v, i) => {
      [o, offset] = v(data, offset);
      object[names ? names[i] : i] = o;
    });

  if (base_names && base_names.length > 0)
    object.__bases = bases;
  return [ object, offset ];
  }

function read_structures_bounded(data, offset, reader, size) {
  let objects = [], o;
  for (let n = 0; n < size; ++n) {
    [o, offset] = reader(data, offset);
    objects.push(o);
    }
  return [ objects, offset ];
  }

function read_structures(data, offset, reader) {
  let size;
  [size, offset] = read_number(data, offset, 'getUint32');
  return read_structures_bounded(data, offset, reader, size);
  }

function read_associative_bounded(data, offset, key_reader, value_reader,
                                  size) {
  let keys, values, map = {};
  [keys, offset] = key_reader(data, offset, size);
  [values, offset] = value_reader(data, offset, size);
  for (let k in keys)
    map[JSON.stringify(keys[k])] = values[k];
  return [ map, offset ];
  }

function read_associative(data, offset, key_reader, value_reader) {
  let size;
  [size, offset] = read_number(data, offset, 'getUint32');
  return read_associative_bounded(data, offset, key_reader, value_reader, size);
  }

function subdivide(source, dest, extents) {
  if (extents.length == 1) {
    for (let i = 0, end = extents[0]; i < end; ++i) {
      dest.push(source.shift());
      }
    return;
    }

  let sublevel = [...extents ];
  for (let i = 0, end = sublevel.shift(); i < end; ++i) {
    let subdest = [];
    subdivide(source, subdest, [...sublevel ]);
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
  return [ dest, offset ];
  }

function read_optional(data, offset, reader) {
  let tag = false;
  [tag, offset] = read_bool(data, offset);
  if (tag)
    [tag, offset] = reader(data, offset);
  else
    tag = null;
  return [ tag, offset ];
  }

function read_variant(data, offset, readers) {
  let index = -1;
  let object;
  [index, offset] = read_number(data, offset, 'getUint32');
  [object, offset] = readers[index](data, offset);
  return [ {index : index, object : object}, offset ];
  }

//////////////////////////////////
// Javascript to native writers //
//////////////////////////////////

function concat_buffer(l, r) {
  if (l.length == 0)
    return r;
  if (r.length == 0)
    return l;
  [l, r] = [ new DataView(l), new DataView(r) ];
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

function write_enum(object, type) { return write_number(object, type); }

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
  let buffer =
      concat_buffer(write_number(object.length, 'setUint32'),
                    write_numbers_bounded(object, type, object.length));
  return buffer;
  }

function from_codepoint(c) {
  if (c <= 0b01111111)
    return [ c ];
  if (c <= 0b011111111111)
    return [ (c >> 6) | 0b11000000, (c & 0b111111) | 0b10000000 ];
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
  buffer = concat_buffer(write_number(buffer.byteLength, 'setUint32'), buffer);
  return buffer;
  }

function write_structure(object, writers, names, base_writers, base_names) {
  let buffer;
  if (typeof writers === 'function')
    writers = [ writers ];
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
  if (size == 0)
    return [];
  let o = [];
  for (let n = 0; n < size; ++n)
    o.push(writer(object[n]));
  let buffer = o.reduce((p, c) => concat_buffer(p, c));
  return buffer;
  }

function write_structures(object, writer) {
  let buffer = concat_buffer(
      write_number(object.length, 'setUint32'),
      write_structures_bounded(
          object, writer,
          Array.isArray(object) ? object.length : Object.keys(object).length));
  return buffer;
  }

function write_associative_bounded(object, key_writer, value_writer, size) {
  let keys = Object.keys(object);
  let buffer = concat_buffer(key_writer(keys.map(JSON.parse), size),
                             value_writer(keys.map(k => object[k]), size));
  return buffer;
  }

function write_associative(object, key_writer, value_writer) {
  let buffer =
      concat_buffer(write_number(Object.keys(object).length, 'setUint32'),
                    write_associative_bounded(object, key_writer, value_writer,
                                              Object.keys(object).length));
  return buffer;
  }

function subcombine(source, dest, extents) {
  if (extents.length == 1) {
    for (let i = 0, end = extents[0]; i < end; ++i)
      dest.push(source.shift());
    return;
    }

  let sublevel = [...extents ];
  for (let i = 0, end = sublevel.shift(); i < end; ++i)
    subcombine(source[i], dest, [...sublevel ]);
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
  let buffer = concat_buffer(write_number(total, 'setUint32'), array);
  return buffer;
  }

function write_optional(object, writer) {
  let buffer = write_bool(object ? true : false);
  if (object)
    buffer = concat_buffer(buffer, writer(object));
  return buffer;
  }

function write_variant(object, writers) {
  return concat_buffer(write_number(object.index, 'setUint32'),
                       writers[object.index](object.object));
  }

//////////////////////////////
// Native to HTML generator //
//////////////////////////////

function create_gatherer() { return d3.dispatch('gather'); }

function dispatch(dispatcher, value) { dispatcher.on('gather', value); }

function subdispatch(dispatcher, subdispatchers, value) {
  dispatcher.on('gather', () => {
    subdispatchers.forEach(s => s.call('gather'));
    value();
  });
  }

function persona_terminal(persona, parent) {
  d3.select(parent).classed(persona, true);
  }

function persona_bool(persona, parent) {
  let node = d3.select(parent)
                 .append('input')
                 .classed(persona, true)
                 .attr('n2w-signature', this.signature)
                 .attr('type', 'checkbox')
                 .attr('value', false)
                 .node();
  if (this.prefill)
    node.checked = this.prefill == true;
  return () => node.checked ? true : false;
  }

function persona_number(persona, parent) {
  let node = d3.select(parent)
                 .append('input')
                 .classed(persona, true)
                 .attr('n2w-signature', this.signature)
                 .attr('type', 'number')
                 .node();
  if (this.prefill)
    node.value = +this.prefill;
  return () => node.value || 0;
  }

function persona_enum_option(persona, parent, value, name) {
  let node = d3.select(parent)
                 .append('option')
                 .classed(persona, true)
                 .attr('n2w-signature', this.signature)
                 .attr('value', value)
                 .text(name)
                 .node();
  if (this.prefill && value == this.prefill)
    node.selected = true;
  return node;
  }

function persona_enum(persona, parent, enums) {
  let select = d3.select(parent)
                   .append('select')
                   .classed(persona, true)
                   .attr('n2w-signature', this.signature)
                   .node();
  return [ () => +select.value, select ];
  }

function persona_char(persona, parent) {
  let node = d3.select(parent)
                 .append('input')
                 .classed(persona, true)
                 .attr('n2w-signature', this.signature)
                 .attr('type', 'text')
                 .node();
  if (this.prefill)
    node.value = this.prefill[0] == '\0' ? '' : this.prefill[0];
  return () => node.value[0] || '\0';
  }

function persona_string(persona, parent) {
  let node = d3.select(parent)
                 .append('input')
                 .classed(persona, true)
                 .attr('n2w-signature', this.signature)
                 .attr('type', 'text')
                 .node();
  if (this.prefill)
    node.value = this.prefill;
  return () => node.value || '';
  }

function persona_structure_baseslabel(persona, parent) {
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

function persona_structure_basesholder(persona, parent) {
  return d3.select(parent).append('tr').classed(persona, true).node();
  }

function persona_structure_baselabel(persona, parent, base_name) {
  return d3.select(parent)
      .append('td')
      .classed(persona, true)
      .text(base_name)
      .node();
  }

function persona_structure_base(persona, parent) {
  return d3.select(parent)
      .append('td')
      .classed(persona, true)
      .classed('n2w-html', true)
      .node();
  }

function persona_structure_baseholder(persona, parent, base_name) {
  return d3.select(parent).append('tr').classed(persona, true).node();
  }

function persona_structure(persona, parent) {
  return d3.select(parent)
      .append('table')
      .classed(persona, true)
      .attr('n2w-signature', this.signature)
      .node();
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
  return d3.select(parent)
      .append('table')
      .classed(persona, true)
      .attr('n2w-signature', this.signature)
      .node();
  }

function persona_container_element(persona, parent) {
  return d3.select(parent)
      .append('tr')
      .classed(persona, true)
      .append('td')
      .classed('n2w-html', true)
      .node();
  }

function persona_container_element_deleter(persona, element, deleter) {
  return d3.select(element.parentElement)
      .append('td')
      .classed(persona, true)
      .append('input')
      .attr('type', 'button')
      .attr('value', '-')
      .on('click',
          () => {
            d3.select(element.parentElement).remove();
            deleter();
          })
      .node();
  }

function persona_container_expander(persona, parent, on_expand) {
  return d3.select(parent)
      .append('tr')
      .classed(persona, true)
      .append('td')
      .append('input')
      .attr('type', 'button')
      .attr('value', '+')
      .on('click', on_expand)
      .node();
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

function persona_map_element_deleter(persona, element, deleter) {
  return d3.select(element.parentElement)
      .insert('td', ':first-child')
      .classed(persona, true)
      .append('input')
      .attr('type', 'button')
      .attr('value', '-')
      .on('click',
          () => {
            d3.select(element.parentElement).remove();
            deleter();
          })
      .node();
  }

function persona_function_args(persona, parent) {
  return d3.select(parent)
      .append('table')
      .classed(persona, true)
      .append('tr')
      .append('td')
      .node();
  }

function persona_function_return(persona, parent) {
  return d3.select(parent.parentElement)
      .append('td')
      .classed(persona, true)
      .node();
  }

function persona_function_exec(persona, parent, func, name) {
  d3.select(parent.parentElement.parentElement)
      .append('tr')
      .classed(persona, true)
      .append('td')
      .attr('colspan', 2)
      .append('input')
      .attr('type', 'button')
      .attr('value', name)
      .on('click', func);
  }

function persona_submodule_entry(persona, parent, name) {
  return d3.select(parent)
      .append('li')
      .classed(persona, true)
      .text(name)
      .node();
  }

let clipboard = {};
let clipboard_signature = {};
function persona_extracter(persona, parent, extracter) {
  let sig = this.signature;
  let extract;
  if (parent.tagName == 'TABLE') {
    d3.select(parent)
        .select('tr.' + persona + '[n2w-signature="' +
                (this.signature || '').replace(/(")/g, '\\$1') + '"]')
        .remove();
    extract = d3.select(parent)
                  .append('tr')
                  .classed(persona, true)
                  .attr('n2w-signature', this.signature)
                  .append('td')
                  .attr('colspan', 2);
  } else if (parent.tagName == 'TD') {
    extract = d3.select(parent);
  } else if (Array.isArray(parent)) {
    extract = d3.select(parent[0].parentElement.parentElement).append('td');
  } else if (parent.tagName == 'DIV') {
    extract = d3.select(parent.parentElement.parentElement).append('td');
    }
  return extract.append('input')
      .classed(persona, true)
      .attr('n2w-signature', this.signature)
      .attr('type', 'button')
      .attr('value', 'extract')
      .on('click',
          () => {
            clipboard = extracter();
            clipboard_signature = sig;
          })
      .node();
  }
function persona_inserter(persona, parent, inserter) {
  let sig = this.signature;
  let insert;
  if (parent.tagName == 'TABLE') {
    d3.select(parent)
        .select('tr.' + persona + '[n2w-signature="' +
                (sig || '').replace(/(")/g, '\\$1') + '"]')
        .remove();
    insert = d3.select(parent)
                 .append('tr')
                 .classed(persona, true)
                 .attr('n2w-signature', sig)
                 .append('td')
                 .attr('colspan', 2);
  } else if (parent.tagName == 'TD') {
    insert = d3.select(parent);
  } else if (Array.isArray(parent)) {
    insert = d3.select(parent[0].parentElement.parentElement).append('td');
  } else if (parent.tagName == 'DIV') {
    insert = d3.select(parent.parentElement.parentElement).append('td');
    }

  return insert.append('input')
      .classed(persona, true)
      .attr('n2w-signature', sig)
      .attr('type', 'button')
      .attr('value', 'insert')
      .on('click',
          () => {
            if (clipboard_signature != sig)
              return;
            if (parent.tagName == 'TABLE')
              d3.select(parent).remove();
            else if (parent.tagName == 'TD') {
              d3.select(parent).selectAll('*').remove();
            } else if (Array.isArray(parent)) {
              d3.select(parent[0].parentElement.parentElement.parentElement)
                  .remove();
            } else if (parent.tagName == 'DIV') {
              d3.select(parent.parentElement.parentElement.parentElement)
                  .remove();
            }
            inserter(clipboard);
          })
      .node();
  }

function persona_submodule(persona, parent) {
  return d3.select(parent).append('ul').classed(persona, true).node();
  }

function extract_doer(dispatcher, subvalue) {
  return function() {
    dispatcher.call('gather');
    return subvalue();
  };
  }

function insert_doer(self, args, sig) {
  return function(value) {
    this.signature = sig;
    this.prefill = value;
    self.apply(this, args);
    this.prefill = null;
  }.bind(this);
  }

function html_void() {}

function html_bool(parent, value, dispatcher) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_bool || persona_bool)('n2w-persona-bool', parent);
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter()));
  (this.persona_extracter || persona_extracter)
      .bind(this)('n2w-persona-extracter', parent,
                  extract_doer(dispatcher, () => value_getter()));
  (this.persona_inserter || persona_inserter)
      .bind(this)('n2w-persona-inserter', parent,
                  insert_doer.bind(this)(this.html_bool, [...arguments ],
                                         this.signature));
  }

function html_number(parent, value, dispatcher) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_number || persona_number)('n2w-persona-number', parent);
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter()));
  (this.persona_extracter || persona_extracter)
      .bind(this)('n2w-persona-extracter', parent,
                  extract_doer(dispatcher, () => value_getter()));
  (this.persona_inserter || persona_inserter)
      .bind(this)('n2w-persona-inserter', parent,
                  insert_doer.bind(this)(this.html_number, [...arguments ],
                                         this.signature));
  }

function html_enum(parent, value, dispatcher, enums) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_enum || persona_enum)('n2w-persona-enum', parent, enums);
  Object.keys(enums)
      .filter(k => enums[k].length > 0)
      .map(k => +k)
      .sort((l, r) => l - r)
      .forEach((k => (this.persona_enum_option || persona_enum_option)(
                    'n2w-persona-enum-option', value_getter[1], k, enums[k]))
                   .bind(this));
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter[0]()));
  (this.persona_extracter || persona_extracter)
      .bind(this)('n2w-persona-extracter', parent,
                  extract_doer(dispatcher, () => value_getter[0]()));
  (this.persona_inserter || persona_inserter)
      .bind(this)('n2w-persona-inserter', parent,
                  insert_doer.bind(this)(this.html_enum, [...arguments ],
                                         this.signature));
  }

function html_char(parent, value, dispatcher) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_char || persona_char)('n2w-persona-char8', parent);
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter()));
  (this.persona_extracter || persona_extracter)
      .bind(this)('n2w-persona-extracter', parent,
                  extract_doer(dispatcher, () => value_getter()));
  (this.persona_inserter || persona_inserter)
      .bind(this)('n2w-persona-inserter', parent,
                  insert_doer.bind(this)(this.html_char, [...arguments ],
                                         this.signature));
  }

function html_char32(parent, value, dispatcher) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_char || persona_char)('n2w-persona-char32', parent);
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter()));
  (this.persona_extracter || persona_extracter)
      .bind(this)('n2w-persona-extracter', parent,
                  extract_doer(dispatcher, () => value_getter()));
  (this.persona_inserter || persona_inserter)
      .bind(this)('n2w-persona-inserter', parent,
                  insert_doer.bind(this)(this.html_char32, [...arguments ],
                                         this.signature));
  }

function html_string(parent, value, dispatcher) {
  (this.persona_terminal || persona_terminal)('n2w-terminal', parent);
  let value_getter =
      (this.persona_string || persona_string)('n2w-persona-string', parent);
  (this.dispatch || dispatch)(dispatcher, () => value(value_getter()));
  (this.persona_extracter || persona_extracter)
      .bind(this)('n2w-persona-extracter', parent,
                  extract_doer(dispatcher, () => value_getter()));
  (this.persona_inserter || persona_inserter)
      .bind(this)('n2w-persona-inserter', parent,
                  insert_doer.bind(this)(this.html_string, [...arguments ],
                                         this.signature));
  }

function html_structure(parent, value, dispatcher, html, names, base_html,
                        base_names) {
  let sig = this.signature;
  let table = (this.persona_structure ||
               persona_structure)('n2w-persona-structure', parent);
  let subvalue = {};
  let subdispatchers = [];
  let basedispatchers = [];

  let bases = {};
  if (base_names && base_names.length > 0) {
    let bases_holder =
        (this.persona_structure_basesholder || persona_structure_basesholder)(
            'n2w-persona-structure-basesholder', table);
    (this.persona_structure_baseslabel || persona_structure_baseslabel)(
        'n2w-persona-structure-baseslabel', bases_holder);
    let bases_data = (this.persona_structure_bases || persona_structure_bases)(
        'n2w-persona-structure-bases', bases_holder);
    base_html.forEach(
        ((h, i) => {
          let basedispatcher = (this.create_gatherer || create_gatherer)();
          basedispatchers.push(basedispatcher);
          let base_holder = (this.persona_structure_baseholder ||
                             persona_structure_baseholder)(
              'n2w-persona-structure-base-holder', bases_data);
          (this.persona_structure_baselabel || persona_structure_baselabel)(
              'n2w-persona-structure-base-label', base_holder, base_names[i]);
          let prefill_saved = this.prefill;
          if (this.prefill)
            this.prefill = this.prefill.__bases[base_names[i]];
          h.bind(this)((this.persona_structure_base || persona_structure_base)(
                           'n2w-persona-structure-base', base_holder),
                       v => bases[base_names[i]] = v, basedispatcher);
          this.prefill = prefill_saved;
        }).bind(this));
    }

  if (typeof(html) == 'function')
    html = [ html ];

  if (Array.isArray(html))
    html.forEach(
        ((h, i) => {
          let subdispatcher = (this.create_gatherer || create_gatherer)();
          subdispatchers.push(subdispatcher);
          let mem_holder =
              (this.persona_structure_memholder || persona_structure_memholder)(
                  'n2w-persona-structure-member-holder', table);
          (this.persona_structure_memlabel ||
           persona_structure_memlabel)('n2w-persona-structure-member-label',
                                       mem_holder, names ? names[i] : i);
          let prefill_saved = this.prefill;
          if (this.prefill)
            this.prefill = this.prefill[names ? names[i] : i];
          h.bind(this)(
              (this.persona_structure_memvalue || persona_structure_memvalue)(
                  'n2w-persona-structure-member', mem_holder),
              v => subvalue[names ? names[i] : i] = v, subdispatcher);
          this.prefill = prefill_saved;
        }).bind(this));

  this.signature = sig;

  (this.subdispatch ||
   subdispatch)(dispatcher, basedispatchers.concat(subdispatchers), () => {
    if (base_names && base_names.length > 0)
      subvalue.__bases = bases;
    value(subvalue);
  });

  let inserter_args = [...arguments ];
  (this.persona_extracter || persona_extracter)
      .bind(this)('n2w-persona-extracter', table,
                  extract_doer(dispatcher, () => subvalue));
  (this.persona_inserter || persona_inserter)
      .bind(this)(
          'n2w-persona-inserter', table,
          insert_doer.bind(this)(this.html_structure, [...arguments ], sig));
  }

function generic_container(parent, value, dispatcher, html, size_or_deleter,
                           suppress_extracter_inserter) {
  let sig = this.signature;
  let table = (this.persona_container ||
               persona_container)('n2w-persona-container', parent);
  let subvalue = [];
  let subdispatchers = [];
  let inserter_args = [...arguments ];

  let extracter_inserter = suppress_extracter_inserter || function(table) {
    (this.persona_extracter || persona_extracter)
        .bind(this)('n2w-persona-extracter', table,
                    extract_doer(
                        dispatcher,
                        () => subvalue.filter(s => s !== __n2w_deleted_value)));
    (this.persona_inserter || persona_inserter)
        .bind(this)('n2w-persona-inserter', table,
                    insert_doer.bind(this)(generic_container.bind(this),
                                           inserter_args, sig));
  }.bind(this);

  if (typeof(size_or_deleter) == 'number')
    for (let i = 0; i < size_or_deleter; ++i) {
      let subdispatcher = (this.create_gatherer || create_gatherer)();
      subdispatchers.push(subdispatcher);
      let prefill_saved = this.prefill;
      if (this.prefill)
        this.prefill = this.prefill[i];
      html.bind(this)(
          (this.persona_container_element ||
           persona_container_element)('n2w-persona-container-element', table),
          v => subvalue[i] = v, subdispatcher);
      this.prefill = prefill_saved;
      }
  else {
    let index = 0;
    let expander =
        (() => {
          let subdispatcher = (this.create_gatherer || create_gatherer)();
          subdispatchers.push(subdispatcher);
          let slot = index++;
          let element =
              (this.persona_container_element || persona_container_element)(
                  'n2w-persona-container-element', table);
          html.bind(this)(element, v => {
            subvalue[slot] =
                subvalue[slot] != __n2w_deleted_value ? v : __n2w_deleted_value;
          }, subdispatcher);
          size_or_deleter.bind(this)(
              'n2w-persona-container-element-deleter', element,
              () => subvalue[slot] = __n2w_deleted_value, slot);

          this.signature = sig;
          extracter_inserter(table);
        }).bind(this);
    (this.persona_container_expander || persona_container_expander)(
        'n2w-persona-container-expander', table, expander);
    let prefill_saved = this.prefill;
    if (this.prefill)
      for (let pf in prefill_saved) {
        this.prefill = prefill_saved[pf];
        expander();
      }
    this.prefill = prefill_saved;
  }

  this.signature = sig;

  (this.subdispatch || subdispatch)(dispatcher, subdispatchers, () => {
    value(subvalue.filter(s => s !== __n2w_deleted_value));
  });

  extracter_inserter(table);
  }

function html_bounded() { generic_container.bind(this)(...arguments); }
function html_sequence() {
  generic_container.bind(this)(...arguments,
                               this.persona_container_element_deleter ||
                                   persona_container_element_deleter);
  }

function html_associative(parent, value, dispatcher, html_key, html_value) {
  let sig = this.signature;
  let subvalue = [];
  let subdispatchers = [];
  let inserter_args = [...arguments ];

  let extracter_inserter = function(table) {
    (this.persona_extracter || persona_extracter)
        .bind(this)(
            'n2w-persona-extracter', table,
            extract_doer(dispatcher,
                         () => subvalue.filter(s => s !== __n2w_deleted_value)
                                   .reduce((p, c) => {
                                     p[JSON.stringify(c[0])] = c[1];
                                     return p;
                                   }, {})));
    (this.persona_inserter || persona_inserter)
        .bind(this)(
            'n2w-persona-inserter', table,
            insert_doer.bind(this)(this.html_associative, inserter_args, sig));
  }.bind(this);

  let prefill_saved = this.prefill;
  if (this.prefill)
    this.prefill = Object.keys(this.prefill).map(k => [k, this.prefill[k]]);
  (this.generic_container || generic_container)
      .bind(this)(
          parent, v => {}, (this.create_gatherer || create_gatherer)(),
          (p, v, d) => {
            let key_value = [];
            subvalue.push(key_value);
            let key_subdispatcher = (this.create_gatherer || create_gatherer)(),
                value_subdispatcher =
                    (this.create_gatherer || create_gatherer)();
            subdispatchers.push(key_subdispatcher);
            subdispatchers.push(value_subdispatcher);
            let prefill_saved = this.prefill;
            if (this.prefill)
              this.prefill = JSON.parse(prefill_saved[0]);
            html_key.bind(this)((this.persona_map_key ||
                                 persona_map_key)('n2w-persona-map-key', p),
                                v => { key_value[0] = v; }, key_subdispatcher);
            if (this.prefill)
              this.prefill = prefill_saved[1];
            html_value.bind(this)((this.persona_map_value || persona_map_value)(
                                      'n2w-persona-map-value', p),
                                  v => { key_value[1] = v; },
                                  value_subdispatcher);
            this.prefill = prefill_saved;
          },
          (p, e, d, s) => {
            (this.persona_map_element_deleter || persona_map_element_deleter)(
                'n2w-persona-map-element-deleter', e,
                () => {subvalue[s] = __n2w_deleted_value});
          },
          extracter_inserter);
  this.prefill = prefill_saved;

  this.signature = sig;

  (this.subdispatch || subdispatch)(dispatcher, subdispatchers, () => {
    value(subvalue.filter(s => s !== __n2w_deleted_value).reduce((p, c) => {
      p[JSON.stringify(c[0])] = c[1];
      return p;
    }, {}));
  });
  }

function persona_optional(persona, parent, switcher) {
  let toggle_row = d3.select(parent)
                       .append('table')
                       .classed(persona, true)
                       .attr('n2w-signature', this.signature)
                       .append('tr');
  toggle_row.append('td')
      .append('input')
      .attr('type', 'checkbox')
      .attr('checked', this.prefill ? true : null)
      .on('click', (d, i, n) => {
        switcher(n[0].checked);
        d3.select(n[0].parentElement.nextElementSibling)
            .style('display', n[0].checked ? null : 'none');
      });
  return toggle_row.append('td')
      .style('display', this.prefill ? null : 'none')
      .append('div')
      .node();
  }

function html_optional(parent, value, dispatcher, html) {
  let switch_value = false;
  let switch_node = (this.persona_optional || persona_optional)
                        .bind(this)('n2w-persona-optional-switch', parent,
                                    v => switch_value = v);
  let subdispatcher = (this.create_gatherer || create_gatherer)();
  let optional_value = null;
  html.bind(this)(switch_node, v => optional_value = (switch_value ? v : null),
                  subdispatcher);

  (this.subdispatch || subdispatch)(dispatcher, [ subdispatcher ],
                                    () => { value(optional_value); });

  (this.persona_extracter || persona_extracter)
      .bind(this)('n2w-persona-extracter', switch_node,
                  extract_doer(dispatcher, () => optional_value));
  (this.persona_inserter || persona_inserter)
      .bind(this)('n2w-persona-inserter', switch_node,
                  insert_doer.bind(this)(this.html_optional, [...arguments ],
                                         this.signature));
  }

function persona_variant(persona, parent, switcher, size) {
  let toggle_row = d3.select(parent)
                       .append('table')
                       .classed(persona, true)
                       .attr('n2w-signature', this.signature)
                       .append('tr');
  let toggler = toggle_row.append('td')
                    .append('input')
                    .attr('type', 'range')
                    .attr('min', 0)
                    .attr('max', size - 1)
                    .attr('step', 1)
                    .attr('value', this.prefill ? this.prefill.index : 0);
  let toggled = toggle_row.append('td');
  for (let i = 0; i < size; ++i)
    toggled.append('div').style(
        'display',
        this.prefill ? (this.prefill.index == i ? null : 'none') : 'none');
  let divs = toggled.selectAll('div').nodes();
  toggler.on('input', (d, i, n) => {
    switcher(+n[0].value);
    divs.map(d3.select).forEach(v => v.style('display', 'none'));
    d3.select(divs[+n[0].value]).style('display', null);
  });
  return divs;
  }

function html_variant(parent, value, dispatcher, html) {
  let switch_value = 0;
  let switch_nodes = (this.persona_variant || persona_variant)
                         .bind(this)('n2w-persona-variant', parent,
                                     v => switch_value = v, html.length);
  let subdispatchers = [];
  let values = [];
  html.forEach((h, i) => {
    let prefill_saved = this.prefill;
    if (this.prefill)
      this.prefill = this.prefill.index == i ? this.prefill.object : null;
    let subdispatcher = (this.create_gatherer || create_gatherer)();
    h.bind(this)(switch_nodes[i], v => values[i] = v, subdispatcher);
    this.prefill = prefill_saved;
    subdispatchers[i] = subdispatcher;
  });
  (this.subdispatch || subdispatch)(
      dispatcher, subdispatchers,
      () => value({index : switch_value, object : values[switch_value]}));

  (this.persona_extracter || persona_extracter)
      .bind(this)(
          'n2w-persona-extracter', switch_nodes,
          extract_doer(dispatcher, () => {
            return {index : switch_value, object : values[switch_value]};
          }));
  (this.persona_inserter || persona_inserter)
      .bind(this)('n2w-persona-inserter', switch_nodes,
                  insert_doer.bind(this)(this.html_variant, [...arguments ],
                                         this.signature));
  }

function html_function(parent, html_args, html_return, name, executor) {
  let value = [], dispatcher = (this.create_gatherer || create_gatherer)();
  let args_node = (this.persona_function_args ||
                   persona_function_args)('n2w-persona-function-args', parent);
  let ret_node = (this.persona_function_return || persona_function_return)(
      'n2w-persona-function-return', args_node);
  html_args.bind(this)(args_node, v => value = v, dispatcher);
  let execute = (() => {
                  value = [];
                  dispatcher.call('gather');
                  executor(Object.keys(value)
                               .map(k => +k)
                               .sort((l, r) => l - r)
                               .reduce(
                                   (p, c) => {
                                     p[c] = value[c];
                                     return p;
                                   },
                                   []),
                           (r => {
                             let generator = this || new N2WGenerator();
                             generator.prefill = r;
                             ret_node.innerHTML = '';
                             html_return.bind(generator)(
                                 ret_node, v => r = v,
                                 (this.create_gatherer || create_gatherer)());
                             generator.prefill = null;
                           }).bind(this));
                  value = [];
                }).bind(this);
  (this.persona_function_exec || persona_function_exec)(
      'n2w-persona-function-execute', args_node, execute, name);
  }

function html_module_directory(module, parent, ws) {
  let submod = (this.persona_submodule ||
                persona_submodule)('n2w-persona-submodule', parent);
  Object.keys(module).forEach(m => {
    let entry =
        persona_submodule_entry('n2w-persona-submodule-entry', submod, m);
    if (typeof(module[m]) == 'function') {
      if (module[m].html)
        pair_service_generator(module[m])(entry, ws);
      return;
    }
    html_module_directory(module[m], entry, ws);
  });
  }

function N2WGenerator() {
  // Customization points for the facade for n2w APIs.
  this.persona_bool = persona_bool.bind(this);
  this.persona_number = persona_number.bind(this);
  this.persona_enum_option = persona_enum_option.bind(this);
  this.persona_enum = persona_enum.bind(this);
  this.persona_char = persona_char.bind(this);
  this.persona_string = persona_string.bind(this);
  this.persona_structure_baseslabel = persona_structure_baseslabel.bind(this);
  this.persona_structure_bases = persona_structure_bases.bind(this);
  this.persona_structure_basesholder = persona_structure_basesholder.bind(this);
  this.persona_structure = persona_structure.bind(this);
  this.persona_structure_baseholder = persona_structure_baseholder.bind(this);
  this.persona_structure_baselabel = persona_structure_baselabel.bind(this);
  this.persona_structure_base = persona_structure_base.bind(this);
  this.persona_structure_memlabel = persona_structure_memlabel.bind(this);
  this.persona_structure_memvalue = persona_structure_memvalue.bind(this);
  this.persona_structure_memholder = persona_structure_memholder.bind(this);
  this.persona_container = persona_container.bind(this);
  this.persona_container_element = persona_container_element.bind(this);
  this.persona_container_element_deleter =
      persona_container_element_deleter.bind(this);
  this.persona_container_expander = persona_container_expander.bind(this);
  this.persona_map_key = persona_map_key.bind(this);
  this.persona_map_value = persona_map_value.bind(this);
  this.persona_map_element_deleter = persona_map_element_deleter.bind(this);
  this.persona_optional = persona_optional.bind(this);
  this.persona_variant = persona_variant.bind(this);
  this.persona_function_args = persona_function_args.bind(this);
  this.persona_function_exec = persona_function_exec.bind(this);
  this.persona_function_return = persona_function_return.bind(this);
  this.persona_submodule = persona_submodule.bind(this);
  this.persona_submodule_entry = persona_submodule_entry.bind(this);
  this.persona_extracter = persona_extracter.bind(this);
  this.persona_inserter = persona_inserter.bind(this);

  // Not so common to customize the following aspects.
  this.persona_terminal = persona_terminal.bind(this);
  // Allow different event dispatcher toolkits.
  this.create_gatherer = create_gatherer.bind(this);
  this.dispatch = dispatch.bind(this);
  this.subdispatch = subdispatch.bind(this);
  // Defines the structure of the interface.
  this.html_bool = html_bool.bind(this);
  this.html_number = html_number.bind(this);
  this.html_enum = html_enum.bind(this);
  this.html_char = html_char.bind(this);
  this.html_char32 = html_char32.bind(this);
  this.html_string = html_string.bind(this);
  this.html_structure = html_structure.bind(this);
  this.html_bounded = html_bounded.bind(this);
  this.html_sequence = html_sequence.bind(this);
  this.html_associative = html_associative.bind(this);
  this.html_optional = html_optional.bind(this);
  this.html_variant = html_variant.bind(this);
  this.html_function = html_function.bind(this);
  this.html_module_directory = html_module_directory.bind(this);
  return this;
  }

/////////////////////////////////////////////////
// WebSocket management for native to web APIs //
/////////////////////////////////////////////////

function create_service(pointer, writer, reader) {
  return function(ws) {
    ws = typeof(ws) == 'function' ? ws() : ws;
    let listener = function(e) {
      ws.removeEventListener('message', listener);
      let ret = reader(new DataView(e.data), 0);
      if (ret)
        this.callback(ret[0]);
      else
        this.callback();
    }.bind(this);
    ws.binaryType = 'arraybuffer';

    let args = [...arguments ];
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

function pair_service_generator(service, generator) {
  return function(parent, ws) {
    service.html.bind(generator || this)(
        parent, (value, executor) => service(ws, ...value).then(executor));
  };
}