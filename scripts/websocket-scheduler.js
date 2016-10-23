function shutdown()
{
  var req = new XMLHttpRequest();
  req.open("POST", "http://" + window.location.host + "/shutdown");
  req.send();
  window.close();
}

var channels = [];

function schedule_websocket(work)
{
  this.worker = function()
  {
    this.args = [];
    this.funcs = [];

    this.then = function(rpc, result)
    {
      if(arguments.length > 2)
        this.funcs.push([rpc, result, Array.prototype.slice.call(arguments, 2)]);
      else
        this.funcs.push([rpc, result]);
      return this;
    }.bind(this);

    this.run = function()
    {
      if(this.funcs.length > 0)
      {
        let func = this.funcs.shift();
        this.args.unshift(this.ws,
          function(d)
          {
            let returned = func[1].bind(this)(d);
            this.args = [];
            if(returned != null)
              this.args = this.args.concat(returned);
            this.run();
          }.bind(this));
        this.args = this.args.concat(func[2]);
        func[0].apply(this, this.args);
      }
      else
        if(this.ws.readyState <= WebSocket.prototype.OPEN)
          channels.unshift(this.ws);
        else
          this.ws = null;
    }.bind(this);

    work.apply(this);
  }.bind(this);

  if(channels.length == 0)
  {
    this.ws = new WebSocket("ws://pthfmd2:9002/");
    this.ws.addEventListener("open", this.worker);
    this.ws.addEventListener("close", function()
    {
      channels.splice(channels.indexOf(this.ws), 1);
    });
  }
  else
  {
    this.ws = channels.shift();
    this.worker();
  }
}
