/// ///////////////////////////
// 从服务器获取数据部分代码
/// ///////////////////////////
var client = {};

function connect_server() {
  // client.ws = new WebSocket('ws://192.168.3.118:7329');
  // client.ws = new WebSocket('ws://woan2007.ticp.io:7329');
  // client.ws = new WebSocket('ws://192.168.1.202:7329');
  // client.ws = new WebSocket('ws://d17.sci-inv.cn:30010');
  // client.ws = new WebSocket('ws://d31.sci-inv.cn:30010');
  client.ws = new WebSocket('ws://127.0.0.1:30010');
  
  // client.ws = new WebSocket('ws://127.0.0.1:7329');
  // client.ws = new WebSocket('ws://192.168.3.118:10810');
  console.log('connection ...');
  // client.ws = new WebSocket('ws://localhost:8888');
  // ws.binaryType = 'blob';
  // client.ws.binaryType = 'arraybuffer';
}

connect_server();

client.ws.onopen = function (e) {
  // client.ws.status = 'open';
  console.log('ws onopen.');
};

client.ws.onclose = function (e) {
  connect_server();
  console.log('ws onclose.');
};

client.ws.onerror = function (evt) {
  console.log('ws onerror.', evt);
};

let askid = 0;
function _makeid() {
  askid++
  // const possible = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'
  // for (let i = 0; i < 20; i++) {
  //   text += possible.charAt(Math.floor(Math.random() * possible.length))
  // }
  return askid.toString();
}

// function make_command_packed(sign, command) {
//   let count = command.length + 1
//   let cmd = '*' + count + '\r\n'
//   cmd += '$' + sign.length + '\r\n' + sign + '\r\n';
//   for (let k = 0; k < command.length; k++) {
//     cmd += '$' + command[k].length + '\r\n' + command[k] + '\r\n';
//   }
//   return cmd
// }

function make_command_packed(sign, input) {
  console.log('<===', typeof input, input.length);
  let command = input.split(' ');

  let cmd = sign + ':{';
  for (let k = 0; k < command.length; k++) {
    let isjson = '"';
    if(command[k][0]==='['||command[k][0]==='{')
    {
      isjson = '';
    }
    if (k === 0)
    {
      cmd += '"cmd":' + isjson + command[k] + isjson;
    }
    else
    if (k === 1)
    {
      cmd += '"subject":' + isjson + command[k] + isjson;
    }
    else
    if (k === 2)
    {
      cmd += '"info":' + isjson + command[k] + isjson;
    }
    else
    {
      cmd += isjson + command[k] + isjson;
    }
    if (k < command.length - 1) 
    {
      cmd += ',';
    }
  }
  if (argv)
  {
    cmd += ']';
  }
  cmd += '}';
  return cmd;
}

// function read_message(message) 
// {
//   if(typeof(message.data)=="string")
//   {  
//     return message.data;
//   }
//   else
//   {  
//     var reader = new FileReader();
//     reader.readAsArrayBuffer(message.data);
//     reader.onload = function (e) 
//     {
//       console.info(reader.result); //ArrayBuffer {}
//       var buf = new Uint8Array(reader.result);
//       console.info(buf);

//       reader.readAsText(new Blob([buf]), 'utf-8');
//       reader.onload = function () 
//       {
//         console.info(reader.result); //中文字符串
//       };
//     }
//   }
// }

client.ws.onmessage = function (message) {

  console.log('===>', typeof message.data, message.data);
  if(typeof(message.data) === "string")
  {  
    let start = message.data.indexOf(':');
    let sign = message.data.substr(0, start);
  
    if (client.wait.commands[sign] !== undefined) {
      let msg = JSON.parse(message.data.substr(start + 1, message.data.length));
      if (msg.tag !== undefined)
      {
        if (msg.info)
        {
          client.wait.replys = msg.tag + ":" + msg.info;
        }
        else
        {
          client.wait.replys = msg.tag;
        }
      }
      else
      {
        client.wait.replys = 'ERROR';
      }
    }
    if (client.wait.multiple)
    {
      let whole = true;
      for (const item in client.wait.commands) 
      {
        // console.log('--- ss --- ', client.wait.commands[item]);
        if (client.wait.replys[client.wait.commands[item]] === undefined) {
          whole = false;
          break;
        }
      }
      if (whole) 
      {
        client.wait.callback(client.wait.replys);
      }  
    }
    else
    {
      client.wait.callback(client.wait.replys);
    }
  }
}

function send_single_command(commands, callback) {
  client.wait = {
    multiple: false,
    callback: callback,
    commands: {},
    replys: {}
  }
  let sign = _makeid();

  client.wait.commands[sign] = sign;

  console.log('<===', commands);

  let sendstr = sign + ':' + commands.cmd;
  // make_command_packed(sign, commands.cmd);

  console.log('<===', sendstr);

  client.ws.send(sendstr);
}
// 多个命令组合 当发出的指令全部返回后，就回调函数
function send_multiple_command(commands, callback) {
  client.wait = {
    multiple: true,
    callback: callback,
    commands: {},
    replys: {}
  }
  for (let index = 0; index < commands.length; index++) {
    let sign = _makeid();
    client.wait.commands[sign] = commands[index].key;

    let sendstr = make_command_packed(sign, commands[index].cmd);

    console.log('m<===', sendstr);

    client.ws.send(sendstr);
  }
  // console.log(client.wait.commands);
}

function client_config_set(key, value) {
  if (window.localStorage) {
    window.localStorage.setItem(key, value);
  }
}

function client_config_get(key) {
  if (window.localStorage) {
    return window.localStorage.getItem(key);
  }
}

function client_config_clear() {
  if (window.localStorage) {
    window.localStorage.clear();
  }
}

function client_config_remove(key) {
  if (window.localStorage) {
    window.localStorage.removeItem(key);
  }
}