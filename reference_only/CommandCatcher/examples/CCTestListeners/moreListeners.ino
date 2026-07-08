
// this will be concatenated with the main .ino file, so no header filees is necessary.

void bListener(char* cmd, char* param) {
  Trace.trace("bl-c", cmd);
  Trace.trace("bl-p", param);
}
