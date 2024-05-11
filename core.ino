#include <LiquidCrystal_I2C.h>

// #define DEBUG_MODE

/*

  This project is based on NodeMCU board

  Mappings:
  I2C 1609 LCD Display
  
  I2C SDA   -> D2
  I2C SDL   -> D1

  Joystick module

  JOY_SW    -> D7
  JOY_AXISY -> A0

  5V USB-C Powered
*/

#pragma region MACRO
/*
*   This region contains required macros 
* e.g. pins, debug mode flag, etc.
*
* Digital pins are board-dependent
*/

#ifdef DEBUG_MODE
  const bool DEBUG = true;
#else
  const bool DEBUG = false;
#endif

#ifdef ARDUINO_ESP8266_NODEMCU_ESP12 | ARDIONO_ESP8266_NODEMCU_ESP12E
  #define JOYSTICK_BTN    D7  
  #define PRESSED         0
  #define RELEASED        1
#else
  #define JOYSTICK_BTN    7
  #define PRESSED         1
  #define RELEASED        0
#endif

#define GUI_MARKER_OFF    "  "
#define GUI_MARKER_ON     "- "

#define JOYSTICK_AXIS_Y   A0

#define SCREEN_WIDTH      16
#define SCREEN_HEIGHT     2
#define NEXT_ITEM         1
#define NEUTRAL           0
#define PREV_ITEM         -1
#pragma endregion MACRO

LiquidCrystal_I2C lcd(0x27, 16, 2);

#pragma region INTERNAL_COMPONENTS
namespace Containers 
{
  template<class T>
  class List
  {
  private:
    T*            items;
    uint8_t size = 0;
  public:
    List(uint8_t size = 0) {
      items = new T[size];
      this->size = size;
    }

    void insert(T item)
    {
      T* buffered = new T[++size];
      for (uint8_t i = 0; i < size - 1; i++)
        buffered[i] = items[i];
      buffered[size - 1] = item;
      delete[] items;
      items = buffered;
    }

    T remove(uint8_t index) {
      if (index >= size)
        return nullptr;
      T* buffered = new T[--size];
      T removed = items[index];
      
      for (uint8_t i = 0; i < index; i++)
        buffered[i] = items[i];
      for (uint8_t i = index + 1; i <= size; i++)
        buffered[i - 1] = items[i];
      delete[] items;
      items = buffered;

      return removed;
    }

    void pop() {
      return remove(size - 1);
    }

    uint8_t getSize() { return size; }

    T& operator[] (uint8_t index) { return items[index]; }
  };
}
#pragma endregion COMPONENTS

//
// <---   INPUT CONTROLLER  ---> //
//

class JoystickController
{
private:
  int8_t previousJoystickState = NEUTRAL;
  bool   previousButtonState   = RELEASED;
  
  // Function calls
  void (*joystick) (int8_t);
  void (*button) ();

  int8_t getJoystickMappedState() 
  {
    return map(analogRead(JOYSTICK_AXIS_Y), 0, 1024, -1, 1);
  }

  void invokeJoystick(int8_t state)
  {
    if (!joystick)
    {
      if (DEBUG)
        Serial.println("Warning! Joystick callback not set!");
      return;
    }

    if (state != NEUTRAL) // neutral state can be ignored
      joystick(state);
  }

  void invokeButton() {
    if (!button) 
    {
      if (DEBUG)
        Serial.println("Warning! Button callback not set!");
      return;
    }
    button();
  }
public:
  JoystickController(void (*joystick) (int8_t) = nullptr, void (*button) () = nullptr) 
  {
    pinMode(JOYSTICK_BTN, INPUT_PULLUP);
    pinMode(JOYSTICK_AXIS_Y, INPUT);

    this-> joystick = joystick;
    this-> button = button;
  }

  void Update() {
    int8_t currentJoystickState = getJoystickMappedState();

    if (currentJoystickState != previousJoystickState)
    {
      previousJoystickState = currentJoystickState;
      invokeJoystick(currentJoystickState);
    }
    bool currentButtonState = digitalRead(JOYSTICK_BTN);

    if (currentButtonState != previousButtonState)
    {
      previousButtonState = currentButtonState;
      if (currentButtonState == PRESSED) 
        invokeButton();
    }
  }
};

class MenuAction
{
private:
  String  label;
  bool    chosen;
protected:
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
    return (String) (String) (chosen ? GUI_MARKER_ON : GUI_MARKER_OFF) + label;
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
  uint8_t currentActive = 0;
  uint8_t currentDisplayStart = 0;

  struct Vector2i 
  {
    uint8_t x;
    uint8_t y;
  };

  void print(Vector2i position, String str) 
  {
    lcd.setCursor(position.x, position.y);
    lcd.print(str);
  }
public:
  Menu()
  {
    menu.insert(MenuAction("Backlight ON", []() { lcd.backlight(); } ));
    menu.insert(MenuAction("Backlight OFF", []() { lcd.noBacklight(); } ));
    menu[currentActive].setActive(true);
  }

  void Display()
  {
    Vector2i start = { 0, 0 };
    for (uint8_t i = 0; i < menu.getSize(); i++) {
      print(start, menu[i].toString());
      start.y++;
    }
  }

  void setActive(int8_t index)
  {
    if (index >= menu.getSize() || index < 0)
    {
      index = abs(index % (int8_t) menu.getSize());
    }
    for (uint8_t i = 0; i < menu.getSize(); i++)
      menu[i].setActive(false);
    menu[index].setActive(true);
    currentActive = index;

    Display();
  }

  uint8_t getActive() { return currentActive; }

  MenuAction& getActiveAction() { return menu[currentActive]; }
};

Menu menu;
JoystickController controller([](int8_t state) { menu.setActive(menu.getActive() + state); }, []() { menu.getActiveAction().Invoke(); });

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