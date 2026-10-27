// GPL-2.0-or-later. Static stack compensation; no detector or gain follower.
// groupOutput is the member sum divided by stack size. Fixed per-member
// gains1/.76/.61/.49 leave larger stacks modestly louder, preserving beating.
  static float stackCompensation(int count){
    return count<=1?1.f:count==2?1.52f:count==4?2.44f:3.92f;
  }
  float normalizeGroup(int,float average)const{return average*stackCompensation(groupSize);}
