static int counter = 0;

extern "C"  int getCounter() { return counter; }

extern "C"  void incCounter() { ++counter; }
