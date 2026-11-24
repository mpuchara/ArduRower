#pragma once
#include <stdint.h>

namespace startup {
enum class Page : uint8_t { Welcome, Users, Modes, Workout, ConfirmSwitch };
enum class Choice : uint8_t { Training, Ghost, Back };
constexpr uint8_t kUserCount=4;
constexpr uint32_t kWelcomeMs=2000;

inline const char *recordNamespace(uint8_t user) {
  static const char *const names[]={"rower-races","rower-u2","rower-u3","rower-u4"};
  return user>=1 && user<=kUserCount?names[user-1]:nullptr;
}

struct Tile {
  int16_t x,y,w,h;
  bool contains(int16_t px,int16_t py) const {
    return px>=x && px<x+w && py>=y && py<y+h;
  }
};

inline Tile tile(Page page,uint8_t index) {
  if(page==Page::Users)
    return {static_cast<int16_t>(12+152*(index%2)),
            static_cast<int16_t>(48+82*(index/2)),144,74};
  if(index==2)return {12,182,296,34};
  return {12,static_cast<int16_t>(58+62*index),296,54};
}

class Navigation {
 public:
  Page page=Page::Welcome;
  uint8_t user=0,focus=0;

  bool measuring() const {return page==Page::Workout || page==Page::ConfirmSwitch;}
  uint8_t options() const {
    switch(page) {
      case Page::Users: return kUserCount;
      case Page::Modes: return 3;
      case Page::ConfirmSwitch: return 2;
      default: return 0;
    }
  }
  void next() {
    const uint8_t count=options();
    if(count)focus=(focus+1)%count;
  }
  void showUsers() {page=Page::Users;user=focus=0;}
  void tick(uint32_t elapsed) {
    if(page==Page::Welcome && elapsed>=kWelcomeMs)showUsers();
  }
  bool selectUser(uint8_t index) {
    if(page!=Page::Users || index>=kUserCount)return false;
    user=index+1;page=Page::Modes;focus=0;
    return true;
  }
  bool begin(Choice mode) {
    if(page!=Page::Modes || mode==Choice::Back)return false;
    page=Page::Workout;focus=0;
    return true;
  }
  bool requestSwitch(bool started,bool paused) {
    if(page!=Page::Workout || (started && !paused))return false;
    if(started){page=Page::ConfirmSwitch;focus=0;}
    else showUsers();
    return true;
  }
  void finishSwitch(bool confirmed,bool paused) {
    if(page!=Page::ConfirmSwitch)return;
    if(confirmed && paused)showUsers();
    else {page=Page::Workout;focus=0;}
  }
};
}
