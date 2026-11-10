"""A small reader for Soong's Android.bp files: modules, variables, maps,
lists, strings, booleans and + between lists or strings."""

import re

_token = re.compile(r'''
    (?P<space>\s+|//[^\n]*|/\*.*?\*/)
  | (?P<string>"(?:[^"\\]|\\.)*")
  | (?P<number>-?\d+)
  | (?P<name>[A-Za-z_][A-Za-z0-9_.]*)
  | (?P<punct>[{}\[\]:,=+()])
''', re.S | re.X)


def tokens(text):
    position = 0
    while position < len(text):
        match = _token.match(text, position)
        if not match:
            raise SyntaxError(f"at {text[position:position + 40]!r}")
        position = match.end()
        kind = match.lastgroup
        if kind != "space":
            yield kind, match.group(kind)


class Parser:
    def __init__(self, text):
        self.tokens = list(tokens(text))
        self.index = 0
        self.variables = {}
        self.modules = []

    def peek(self):
        return self.tokens[self.index] if self.index < len(self.tokens) else (None, None)

    def take(self, value=None):
        token = self.tokens[self.index]
        if value is not None and token[1] != value:
            raise SyntaxError(f"expected {value}, got {token}")
        self.index += 1
        return token

    def parse(self):
        while self.index < len(self.tokens):
            _, name = self.take()
            kind, value = self.peek()
            if value == "=":
                self.take("=")
                self.variables[name] = self.expression()
            elif value == "+":
                self.take("+")
                self.take("=")
                self.variables[name] = self.variables[name] + self.expression()
            else:
                properties = self.value()
                properties["@type"] = name
                self.modules.append(properties)
        return self

    def expression(self):
        result = self.value()
        while self.peek()[1] == "+":
            self.take("+")
            right = self.value()
            result = result + right
        return result

    def value(self):
        kind, value = self.take()
        if kind == "string":
            return bytes(value[1:-1], "utf-8").decode("unicode_escape")
        if kind == "number":
            return int(value)
        if value == "true":
            return True
        if value == "false":
            return False
        if value == "[":
            items = []
            while self.peek()[1] != "]":
                items.append(self.expression())
                if self.peek()[1] == ",":
                    self.take(",")
            self.take("]")
            return items
        if value in ("{", "("):
            close = "}" if value == "{" else ")"
            result = {}
            while self.peek()[1] != close:
                _, key = self.take()
                self.take(":" if self.peek()[1] == ":" else "=")
                result[key] = self.expression()
                if self.peek()[1] == ",":
                    self.take(",")
            self.take(close)
            return result
        if kind == "name":
            return self.variables[value]
        raise SyntaxError(f"unexpected {value!r}")


def read(path):
    return Parser(open(path).read()).parse()


def module(parsed, name):
    for found in parsed.modules:
        if found.get("name") == name:
            return found
    raise KeyError(name)
