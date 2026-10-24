
/*
 *
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include "command.h"
#include "tokenizer.h"
#include <cctype>
#include <fcntl.h>
#include <glob.h>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <wordexp.h>

void parse(std::vector<Token> &tokens) {
  Command &cmd = Command::_currentCommand;
  SimpleCommand *simple = new SimpleCommand();
  bool error = false;

  for (size_t i = 0; i < tokens.size() && !error; ++i) {
    Token &t = tokens[i];
    switch (t.type) {
    case TOKEN_EOF:
      i = tokens.size();
      break;

    case TOKEN_COMMAND:
    case TOKEN_ARGUMENT:
      simple->insertArgument(strdup(t.value.c_str()));
      break;

    case TOKEN_PIPE:
      if (simple->_numberOfArguments == 0) {
        fprintf(stderr, "myshell: syntax error near unexpected token `|'\n");
        error = true;
      } else {
        cmd.insertSimpleCommand(simple);
        simple = new SimpleCommand();
      }
      break;

    case TOKEN_REDIRECT:
    case TOKEN_APPEND:
    case TOKEN_INPUT:
    case TOKEN_ERROR:
    case TOKEN_REDIRECT_AND_ERROR: {
      if (i + 1 >= tokens.size()) {
        fprintf(stderr,
                "myshell: syntax error: missing filename after redirection\n");
        error = true;
        break;
      }
      TokenType nt = tokens[i + 1].type;
      if (nt != TOKEN_COMMAND && nt != TOKEN_ARGUMENT) {
        fprintf(stderr,
                "myshell: syntax error: missing filename after redirection\n");
        error = true;
        break;
      }
      ++i;
      const std::string &fname = tokens[i].value;
      if (t.type == TOKEN_REDIRECT) {
        cmd._outFile = strdup(fname.c_str());
        cmd._append = 0;
      } else if (t.type == TOKEN_APPEND) {
        cmd._outFile = strdup(fname.c_str());
        cmd._append = 1;
      } else if (t.type == TOKEN_INPUT) {
        cmd._inputFile = strdup(fname.c_str());
      } else if (t.type == TOKEN_ERROR) {
        cmd._errFile = strdup(fname.c_str());
      } else {
        cmd._outFile = strdup(fname.c_str());
        cmd._errFile = strdup(fname.c_str());
        cmd._out_error = 1;
        cmd._append = 1;
      }
      break;
    }

    case TOKEN_BACKGROUND:
      cmd._background = 1;
      break;
    }
  }

  if (error) {
    for (int j = 0; j < simple->_numberOfArguments; ++j)
      free(simple->_arguments[j]);
    free(simple->_arguments);
    free(simple);
    cmd.clear();
    return;
  }

  if (simple->_numberOfArguments == 0) {
    if (cmd._numberOfSimpleCommands > 0) {
      fprintf(stderr,
              "myshell: syntax error: unexpected end of input after pipe\n");
    }
    free(simple->_arguments);
    free(simple);
    cmd.clear();
    return;
  }

  cmd.insertSimpleCommand(simple);
  cmd.execute();
}

SimpleCommand::SimpleCommand() {
  _numberOfAvailableArguments = 5;
  _numberOfArguments = 0;
  _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
  _arguments[0] = NULL;
}

void SimpleCommand::insertArgument(char *argument) {
  if (_numberOfAvailableArguments == _numberOfArguments + 1) {
    _numberOfAvailableArguments *= 2;
    _arguments = (char **)realloc(_arguments,
                                  _numberOfAvailableArguments * sizeof(char *));
  }

  _arguments[_numberOfArguments] = argument;
  _arguments[_numberOfArguments + 1] = NULL;

  _numberOfArguments++;
}

Command::Command() {
  _numberOfAvailableSimpleCommands = 1;
  _simpleCommands = (SimpleCommand **)malloc(_numberOfAvailableSimpleCommands *
                                             sizeof(SimpleCommand *));

  _numberOfSimpleCommands = 0;
  _outFile = 0;
  _inputFile = 0;
  _errFile = 0;
  _out_error = 0;
  _append = 0;
  _background = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand) {
  if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands) {
    _numberOfAvailableSimpleCommands *= 2;
    _simpleCommands = (SimpleCommand **)realloc(
        _simpleCommands,
        _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
  }

  _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
  _numberOfSimpleCommands++;
}

void Command::clear() {
  for (int i = 0; i < _numberOfSimpleCommands; i++) {
    for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++) {
      free(_simpleCommands[i]->_arguments[j]);
    }
    free(_simpleCommands[i]->_arguments);
    free(_simpleCommands[i]);
  }

  if (_outFile)
    free(_outFile);
  if (_inputFile)
    free(_inputFile);
  if (_errFile)
    free(_errFile);

  _numberOfSimpleCommands = 0;
  _outFile = 0;
  _inputFile = 0;
  _errFile = 0;
  _out_error = 0;
  _append = 0;
  _background = 0;
}

void Command::print() {
  printf("\n\n");
  printf("              COMMAND TABLE                \n");
  printf("\n");
  printf("  #   Simple Commands\n");
  printf("  --- ----------------------------------------------------------\n");

  for (int i = 0; i < _numberOfSimpleCommands; i++) {
    printf("  %-3d ", i);
    for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++) {
      printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
    }
  }

  printf("\n\n");
  printf("  Output       Input        Error        Err&Out       Background\n");
  printf(
      "  ------------ ------------ ------------ ------------ ------------\n");
  printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
         _inputFile ? _inputFile : "default", _errFile ? _errFile : "default",
         (_out_error == 1 && _errFile) ? _errFile : "default",
         _background ? "YES" : "NO");
  printf("\n\n");
  fflush(stdout);
}

void Command::execute() {
  if (_numberOfSimpleCommands == 0) {
    clear();
    return;
  }

  print();

  int numCmds = _numberOfSimpleCommands;
  int prevRead = -1;
  pid_t lastPid = -1;

  for (int i = 0; i < numCmds; ++i) {
    int pipefd[2] = {-1, -1};
    bool hasNext = (i < numCmds - 1);

    if (hasNext && pipe(pipefd) < 0) {
      perror("pipe");
      if (prevRead != -1)
        close(prevRead);
      clear();
      return;
    }

    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      if (prevRead != -1)
        close(prevRead);
      if (hasNext) {
        close(pipefd[0]);
        close(pipefd[1]);
      }
      clear();
      return;
    }

    if (pid == 0) {
      signal(SIGINT, SIG_DFL);

      /* stdin: previous pipe wins over _inputFile */
      if (prevRead != -1) {
        dup2(prevRead, STDIN_FILENO);
        close(prevRead);
      } else if (_inputFile) {
        int fd = open(_inputFile, O_RDONLY);
        if (fd < 0) {
          fprintf(stderr, "myshell: %s: %s\n", _inputFile, strerror(errno));
          _exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
      }

      /* stdout + stderr */
      if (_out_error && _errFile) {
        int flags = O_WRONLY | O_CREAT | (_append ? O_APPEND : O_TRUNC);
        int fd = open(_errFile, flags, 0644);
        if (fd < 0) {
          fprintf(stderr, "myshell: %s: %s\n", _errFile, strerror(errno));
          _exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
      } else {
        /* stdout: next pipe wins over _outFile */
        if (hasNext) {
          close(pipefd[0]);
          dup2(pipefd[1], STDOUT_FILENO);
          close(pipefd[1]);
        } else if (_outFile) {
          int flags = O_WRONLY | O_CREAT | (_append ? O_APPEND : O_TRUNC);
          int fd = open(_outFile, flags, 0644);
          if (fd < 0) {
            fprintf(stderr, "myshell: %s: %s\n", _outFile, strerror(errno));
            _exit(1);
          }
          dup2(fd, STDOUT_FILENO);
          close(fd);
        }
        if (_errFile) {
          int fd = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd < 0) {
            fprintf(stderr, "myshell: %s: %s\n", _errFile, strerror(errno));
            _exit(1);
          }
          dup2(fd, STDERR_FILENO);
          close(fd);
        }
      }

      execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
      fprintf(stderr, "myshell: %s: %s\n", _simpleCommands[i]->_arguments[0],
              strerror(errno));
      _exit(1);
    }

    /* parent: drop ends we don't use, then remember read end */
    if (prevRead != -1)
      close(prevRead);
    if (hasNext) {
      close(pipefd[1]);
      prevRead = pipefd[0];
    } else {
      lastPid = pid;
    }
  }

  if (lastPid > 0) {
    int status;
    while (waitpid(lastPid, &status, 0) < 0 && errno == EINTR) {
    }
  }

  clear();
}

void Command::prompt() {
  std::string input;
  while (true) {
    printf("myshell> ");
    fflush(stdout);
    if (!std::getline(std::cin, input)) {
      printf("\n");
      break;
    }
    std::vector<Token> tokens = tokenize(input);
    parse(tokens);
  }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main() {
  Command::_currentCommand.prompt();
  return 0;
}
