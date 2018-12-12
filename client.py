from socket import *
import sys

SERVER_NAME = 'localhost'
SERVER_PORT = 3000


def connect():
    # Create a socket of Address family AF_INET.
    sock = socket(AF_INET, SOCK_STREAM)
    # Client socket connection to the server
    sock.connect((SERVER_NAME, SERVER_PORT))

    return sock


def send(sock, message):
    sock.send(bytearray(message, 'utf-8'))


def recv(sock):
    return sock.recv(1024).decode('utf-8')


def list(sock):
    send(sock, '-')
    result = recv(sock).rstrip()
    if result == '':
        return []
    else:
        return result.split(',')[0:-1]


def last_list(sock):
    xs = list(sock)
    if len(xs) == 0:
        return ''
    else:
        return xs[-1]


def ask(prompt=':-p'):
    return input('({}) '.format(prompt))


def prompt_on_last(sock):
    last = last_list(sock)
    if last == '':
        return ask()
    else:
        return ask(last)

def isValidMsg(message):
    if "\\" in message[0]:
        cmd_ws = message.split(' ', 1)[0].upper() #With slash
        cmd_wos = cmd_ws[1:] #Without slash
        cmds = ["JOIN", "ROOMS", "LEAVE", "WHO", "HELP", "NICKNAME"]

        if cmd_wos not in cmds:
            return False, '"' + cmd_ws + '" command not recognized'

        rest_of_msg = message.split(' ', 1)[1:]

        if cmd_wos == "JOIN" and len(rest_of_msg) > 2:
            return False, "Improper Usage: \\JOIN <nickname> <room>"

        if cmd_wos == "ROOMS" and len(rest_of_msg) > 0:
            return False, "Improper Usage: \\ROOMS"

        if cmd_wos == "LEAVE" and len(rest_of_msg) > 0:
            return False, "Improper Usage: \\LEAVE"

        if cmd_wos == "WHO" and len(rest_of_msg) > 0:
            return False, "Improper Usage: \\WHO"

        if cmd_wos == "HELP" and len(rest_of_msg) > 0:
            return False, "Improper Usage: \\HELP"

        if cmd_wos == "NICKNAME" and len(rest_of_msg) <= 0:
            return False, "Improper Usage: \\nickname <message>"


        message = cmd_ws + ' ' + ' '.join(rest_of_msg)

    return True, message



def client():
    if len(sys.argv) > 2:
        print("Improper usage: python3 <file> <OPTIONAL: file.txt>")
        sys.exit()

    connection = connect()

    sentence = None
    lines = None
    isFile = False
    file_index = 0

    if len(sys.argv) > 1:
        isFile = True
        with open(sys.argv[1]) as f:
            lines = [line.rstrip('\n') for line in f] 
            sentence = lines[file_index]
    else:
        sentence = prompt_on_last(connection)

    while sentence != 'quit' or (isFile and file_index < len(lines)):
        cmd_ok, string = isValidMsg(sentence)

        if(cmd_ok):
            sentence = string
            print("Sending: " + string)
            send(connection, sentence)
            response = recv(connection)
            print(response.strip())
        else:
            print(string)

        file_index = file_index+1
        sentence = lines[file_index] if isFile else prompt_on_last(connection)


client()