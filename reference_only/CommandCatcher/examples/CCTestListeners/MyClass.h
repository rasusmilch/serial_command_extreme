
// header file for a class that will ad its own listener during initialization.

class MyClass  : public CommandListener {
  public:
  MyClass();
  void doSomething();
  static void notifyStatic(char* cmd, char* param);
  void notify(char* cmd, char* param) override;
};