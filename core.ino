#include <LiquidCrystal_I2C.h>

#ifdef DEBUG_MODE
const bool DEBUG = true;
#else
const bool DEBUG = false;
#endif

#define JOYSTICK_BTN D7
#define JOYSTICK_AXIS_Y A0

#define SCREEN_WIDTH 16
#define SCREEN_HEIGHT 2

#define PRESSED   0
#define RELEASED  1

#define NEXT_ITEM   1
#define NEUTRAL     0
#define PREV_ITEM   -1

LiquidCrystal_I2C lcd(0x27, 16, 2);

//
// <--  List controller  ---> //
// 
// Created to ease array implementation

namespace Containers 
{
  template<class T>
  class List
  {
  private:
    T*            items;
    unsigned int  size = 0;
  public:
    List(unsigned int size = 0) {
      items = new T[size];
      this->size = size;
    }

    void insert(T item)
    {
      T* buffered = new T[++size];
      for (unsigned int i = 0; i < size - 1; i++)
        buffered[i] = items[i];
      buffered[size - 1] = item;
      delete[] items;
      items = buffered;
    }

    T remove(unsigned int index) {
      if (index >= size)
        return nullptr;
      T* buffered = new T[--size];
      T removed = items[index];
      
      for (unsigned int i = 0; i < index; i++)
        buffered[i] = items[i];
      for (unsigned int i = index + 1; i <= size; i++)
        buffered[i - 1] = items[i];
      delete[] items;
      items = buffered;

      return removed;
    }

    void pop() {
      return remove(size - 1);
    }

    unsigned int getSize() { return size; }

    T& operator[] (unsigned int index) { return items[index]; }
  };
}

//
// <---   INPUT CONTROLLER  ---> //
//

class JoystickController
{
private:
  short previous = NEUTRAL;
  short buttonPrev = RELEASED;
  void (*callback) (short);
  void (*button) ();

  short tick() 
  {
    return map(analogRead(JOYSTICK_AXIS_Y), 0, 1024, -1, 1);
  }

  void Invoke(short state)
  {
    if (!callback)
    {
      Serial.println("Warning! Callback not set!");
      return;
    }

    if (state != NEUTRAL)
      callback(state);
  }

  void Button() {
    if (button) button();
  }
public:
  JoystickController(void (*callback) (short) = nullptr, void (*button) () = nullptr) 
  {
    pinMode(JOYSTICK_BTN, INPUT_PULLUP);
    pinMode(JOYSTICK_AXIS_Y, INPUT);
    this-> callback = callback;
    this-> button = button;
  }

  void Update() {
    short current = tick();

    if (current != previous)
    {
      Invoke(current);
      previous = current;
    }
    short btnState = digitalRead(JOYSTICK_BTN);

    if (btnState != buttonPrev)
    {
      buttonPrev = btnState;
      if (btnState == PRESSED) Button();
    }
  }
};

class MenuAction
{
private:
  String  label;
  bool    chosen;
  void (*action)();
public:
  MenuAction(String label = "none", void(*action)() = nullptr, bool chosen = false)
  {
    this-> label = label;
    this-> chosen = chosen;
    this->action = action;
  }

  void setActive(bool state) { this-> chosen = state; }

  String toString() 
  {
    return (String) "[" + (String) (chosen ? "*] " : " ] ") + label;
  }

  void Invoke()
  {
    if (action) action();  
  }
};

class Menu 
{
private:
  Containers::List<MenuAction>  menu;
  unsigned int currentActive = 0;

  struct Vector2i 
  {
    unsigned int x;
    unsigned int y;
  };

  void print(Vector2i position, String str) 
  {
    lcd.setCursor(position.x, position.y);
    lcd.print(str);
  }
public:
  Menu()
  {
    menu.insert(MenuAction("List 1", []() { lcd.backlight(); } ));
    menu.insert(MenuAction("List 2", []() { lcd.noBacklight(); } ));
    menu[currentActive].setActive(true);
  }

  void Display()
  {
    Vector2i start = { 0, 0 };
    for (unsigned int i = 0; i < menu.getSize(); i++) {
      print(start, menu[i].toString());
      start.y++;
    }
  }

  void setActive(int index)
  {
    if (index >= menu.getSize() || index < 0)
    {
      index = abs(index % (int) menu.getSize());
    }
    for (unsigned int i = 0; i < menu.getSize(); i++)
      menu[i].setActive(false);
    menu[index].setActive(true);
    currentActive = index;

    Display();
  }

  unsigned int getActive() { return currentActive; }

  MenuAction& getActiveAction() { return menu[currentActive]; }
};

Menu menu;
JoystickController controller([](short state) { menu.setActive(menu.getActive() + state); }, []() { menu.getActiveAction().Invoke(); });

void setup()
{
  lcd.init();
  lcd.backlight();

  menu.Display();

  Serial.begin(9600);
}

void loop() 
{
  controller.Update();
}