var net = require('net');
var readline = require('readline');

var listMessage = true;

var first = true;

var client = new net.Socket();
client.connect(3000, '127.0.0.1', () => {
    console.log('Connected to echo server.');
    listMessage = true;
    client.write('-');
});

client.on('data', (data) => {
    let res = data.toString();
    if (listMessage) {
        let xs = res.trimRight().split(','),
            len = xs.length;
        if (len > 1) {
            res = xs[len - 2];
            if (!first)
                console.log(xs.join(','));
        } else {
            res = ':-p'
        }
        listMessage = false;
        first = false;
    } else {
        console.log(res);
    }
    recursiveAsyncReadLine(client, res)
});

client.on('close', () => {
    console.log('Connection closed');
});

var rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

var recursiveAsyncReadLine = (client, prompt) => {
    rl.question(`(${prompt}) `, (answer) => {
        if (answer == 'quit') {
            client.destroy();
            return rl.close();
        } else if (answer == '-') {
            listMessage = true;
            client.write(answer);
        } else {
            client.write(answer);
        }
    });
};