function dummy_func(ws, callback, arg0, arg1, arg2, arg3) {
  ws.binaryType = "arraybuffer";

  let return_value = [];
  let states = [
    function(data) {
      return_value[0] = [];
      states.shift()(data);

    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][0] = Array.prototype.slice.call(new Float32Array(data));
      let expected = states.shift();
      if(return_value[0][0].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][0].length);

    },

    function(data) {
      return_value[0][1] = Number.parseFloat(data);
    },

    function(data) {
      return_value[0][2] = [];
      let b_length = Number.parseInt(data);
      for(let b = b_length - 1; b >=0; --b) {
        states = [
    function(data) {
      return_value[0][2][b] = [];
      states.shift()(data);

    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][2][b][0] = data;
      let expected = states.shift();
      if(return_value[0][2][b][0].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][2][b][0].length);

    },

    function(data) {
      return_value[0][2][b][1] = [];
      let d_length = Number.parseInt(data);
      for(let d = d_length - 1; d >=0; --d) {
        states = [
    function(data) {
      return_value[0][2][b][1][d] = [];
      let e_length = Number.parseInt(data);
      for(let e = e_length - 1; e >=0; --e) {
        states = [
    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][2][b][1][d][e] = Array.prototype.slice.call(new Float64Array(data));
      let expected = states.shift();
      if(return_value[0][2][b][1][d][e].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][2][b][1][d][e].length);

    }
        ].concat(states);
      }

    }
        ].concat(states);
      }

    },

    function(data) {
      return_value[0][2][b][2] = Number.parseFloat(data);
    }
        ].concat(states);
      }

    },

    function(data) {
      return_value[0][3] = [];
      states.shift()(data);

    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][3][0] = Array.prototype.slice.call(new Float64Array(data));
      let expected = states.shift();
      if(return_value[0][3][0].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][3][0].length);

    },

    function(data) {
      return_value[0][3][1] = [];
      states.shift()(data);

    },

    function(data) {
      return_value[0][3][1][0] = Number.parseFloat(data);
    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][3][1][1] = data;
      let expected = states.shift();
      if(return_value[0][3][1][1].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][3][1][1].length);

    },

    function(data) {
      return_value[0][3][1][2] = Number.parseFloat(data);
    },

    function(data) {
      return_value[0][3][2] = Number.parseFloat(data);
    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][3][3] = data;
      let expected = states.shift();
      if(return_value[0][3][3].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][3][3].length);

    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][3][4] = Array.prototype.slice.call(new Uint32Array(data));
      let expected = states.shift();
      if(return_value[0][3][4].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][3][4].length);

    },


  ];

  let reader = function(event) {
    states.shift()(event.data);

    if(states.length == 0) {
      ws.removeEventListener("message", reader);
      callback(return_value[0]);
    }
  };

  ws.addEventListener("message", reader);

  let args = [arg0, arg1, arg2, arg3];
  let view;

  ws.send("Calling 4283776()...");

  view = new Float32Array(args[0]);
  ws.send(view.length.toString());
  ws.send(view.buffer);

  ws.send(args[1].toString());

  ws.send(args[2].length.toString());
  for(let a in args[2]) {
  ws.send(args[2][a][0].length.toString());
  ws.send(args[2][a][0]);

  ws.send(args[2][a][1].length.toString());
  for(let c in args[2][a][1]) {
  ws.send(args[2][a][1][c].length.toString());
  for(let d in args[2][a][1][c]) {
  view = new Float64Array(args[2][a][1][c][d]);
  ws.send(view.length.toString());
  ws.send(view.buffer);


  }


  }

  ws.send(args[2][a][2].toString());


  }

  view = new Float64Array(args[3][0]);
  ws.send(view.length.toString());
  ws.send(view.buffer);

  ws.send(args[3][1][0].toString());

  ws.send(args[3][1][1].length.toString());
  ws.send(args[3][1][1]);

  ws.send(args[3][1][2].toString());

  ws.send(args[3][2].toString());

  ws.send(args[3][3].length.toString());
  ws.send(args[3][3]);

  view = new Uint32Array(args[3][4]);
  ws.send(view.length.toString());
  ws.send(view.buffer);


}

function void_dummy_func(ws, callback, arg0, arg1) {
  ws.binaryType = "arraybuffer";

  let return_value = [];
  let states = [
    function(data) {
      if(data !== "void")
        throw "Was expecting void function";
      return_value[0] = "void";
}
  ];

  let reader = function(event) {
    states.shift()(event.data);

    if(states.length == 0) {
      ws.removeEventListener("message", reader);
      callback(return_value[0]);
    }
  };

  ws.addEventListener("message", reader);

  let args = [arg0, arg1];
  let view;

  ws.send("Calling 4283968()...");

  ws.send(args[0].length.toString());
  ws.send(args[0]);

  view = new Int8Array(args[1]);
  ws.send(view.length.toString());
  ws.send(view.buffer);


}

function bool_dummy_func(ws, callback, arg0, arg1) {
  ws.binaryType = "arraybuffer";

  let return_value = [];
  let states = [
    function(data) {
      if(data !== "true" && data !== "false")
        throw "Expecting a boolean value string";
  
      return_value[0] = (data === "true" ? true : false);
    },


  ];

  let reader = function(event) {
    states.shift()(event.data);

    if(states.length == 0) {
      ws.removeEventListener("message", reader);
      callback(return_value[0]);
    }
  };

  ws.addEventListener("message", reader);

  let args = [arg0, arg1];
  let view;

  ws.send("Calling 4283984()...");

  ws.send(args[0].length.toString());
  ws.send(args[0]);

  ws.send(args[1].toString());


}

function home_directory(ws, callback) {
  ws.binaryType = "arraybuffer";

  let return_value = [];
  let states = [
    function(data) {
      return_value[0] = [];
      let a_length = Number.parseInt(data);
      for(let a = a_length - 1; a >=0; --a) {
        states = [
    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][a] = data;
      let expected = states.shift();
      if(return_value[0][a].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][a].length);

    }
        ].concat(states);
      }

    },


  ];

  let reader = function(event) {
    states.shift()(event.data);

    if(states.length == 0) {
      ws.removeEventListener("message", reader);
      callback(return_value[0]);
    }
  };

  ws.addEventListener("message", reader);

  let args = [];
  let view;

  ws.send("Calling 4294464()...");


}

function list_files(ws, callback, arg0, arg1) {
  ws.binaryType = "arraybuffer";

  let return_value = [];
  let states = [
    function(data) {
      return_value[0] = [];
      states.shift()(data);

    },

    function(data) {
      return_value[0][0] = [];
      states.shift()(data);

    },

    function(data) {
      return_value[0][0][0] = [];
      let c_length = Number.parseInt(data);
      for(let c = c_length - 1; c >=0; --c) {
        states = [
    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][0][0][c] = data;
      let expected = states.shift();
      if(return_value[0][0][0][c].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][0][0][c].length);

    }
        ].concat(states);
      }

    },

    function(data) {
      return_value[0][0][1] = Number.parseFloat(data);
    },

    function(data) {
      return_value[0][0][2] = Number.parseFloat(data);
    },

    function(data) {
      return_value[0][0][3] = Number.parseFloat(data);
    },

    function(data) {
      return_value[0][1] = [];
      let b_length = Number.parseInt(data);
      for(let b = b_length - 1; b >=0; --b) {
        states = [
    function(data) {
      return_value[0][1][b] = [];
      states.shift()(data);

    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][1][b][0] = data;
      let expected = states.shift();
      if(return_value[0][1][b][0].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][1][b][0].length);

    },

    function(data) {
      return_value[0][1][b][1] = Number.parseFloat(data);
    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][1][b][2] = data;
      let expected = states.shift();
      if(return_value[0][1][b][2].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][1][b][2].length);

    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][1][b][3] = data;
      let expected = states.shift();
      if(return_value[0][1][b][3].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][1][b][3].length);

    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][1][b][4] = data;
      let expected = states.shift();
      if(return_value[0][1][b][4].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][1][b][4].length);

    },

    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][1][b][5] = data;
      let expected = states.shift();
      if(return_value[0][1][b][5].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][1][b][5].length);

    }
        ].concat(states);
      }

    },


  ];

  let reader = function(event) {
    states.shift()(event.data);

    if(states.length == 0) {
      ws.removeEventListener("message", reader);
      callback(return_value[0]);
    }
  };

  ws.addEventListener("message", reader);

  let args = [arg0, arg1];
  let view;

  ws.send("Calling 4294656()...");

  ws.send(args[0].length.toString());
  for(let a in args[0]) {
  ws.send(args[0][a].length.toString());
  ws.send(args[0][a]);


  }

  ws.send(args[1].toString());


}

function list_roots(ws, callback) {
  ws.binaryType = "arraybuffer";

  let return_value = [];
  let states = [
    function(data) {
      return_value[0] = [];
      let a_length = Number.parseInt(data);
      for(let a = a_length - 1; a >=0; --a) {
        states = [
    function(data) {
      return_value[0][a] = [];
      let b_length = Number.parseInt(data);
      for(let b = b_length - 1; b >=0; --b) {
        states = [
    function(data) {
      let length = Number.parseInt(data);
      let top = states.shift();
      states.unshift(top, length);
    },
    function(data) {
      return_value[0][a][b] = data;
      let expected = states.shift();
      if(return_value[0][a][b].length != expected)
        console.log("Expected " + expected + ", got " + return_value[0][a][b].length);

    }
        ].concat(states);
      }

    }
        ].concat(states);
      }

    },


  ];

  let reader = function(event) {
    states.shift()(event.data);

    if(states.length == 0) {
      ws.removeEventListener("message", reader);
      callback(return_value[0]);
    }
  };

  ws.addEventListener("message", reader);

  let args = [];
  let view;

  ws.send("Calling 4304160()...");


}



