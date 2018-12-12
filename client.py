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
        send(connection, sentence)
        response = recv(connection)
        print(response.strip())
        sentence = lines[++file_index] if isFile else prompt_on_last(connection)


client()
