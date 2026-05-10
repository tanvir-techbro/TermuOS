#pragma once

typedef void (*shell_output_fn)(char c);

void shell_set_output(shell_output_fn fn);
void shell_run(void);
void shell_execute(char *line);
