#include "switcher-layout-policy-latest.h"
#include <iostream>
#include <limits>
using namespace NativeSwitcherLayout;
int checks{},failures{};
void check(bool value,const char* reason) {++checks;if(!value){++failures;std::cerr<<"FAIL "<<reason<<'\n';}}
std::vector<Item> same(unsigned count,double aspect=1.6){return std::vector<Item>(count,{aspect,42,2});}
int main(){
    auto six=Choose(1538,864,same(6));
    check(six.valid && six.rows==2,"six landscape windows use two rows");
    check(six.cardHeight>=312,"six previews are materially larger than the rejected 182-DIP row");
    check(six.width<=1538 && six.height<=864,"six fit available width and height");
    auto one=Choose(1538,864,same(1));
    check(one.cardHeight>=600 && one.cardHeight<=864*.75,"lone window uses a large display-relative preview");
    check(one.rows==1 && one.width<1538 && one.height<864,"canvas follows the lone preview, retaining desktop context");
    auto two=Choose(1538,864,same(2));
    check(two.valid && two.cardHeight>360 && two.cardHeight<=one.cardHeight,"two previews also escape the old fixed cap");
    auto four=Choose(1538,864,same(4));
    check(four.valid && four.rows==2 && four.cardHeight>=360,"four previews grow in two rows");
    check(six.cardHeight<=four.cardHeight && six.cardHeight>=312,"denser list retains useful accepted multi-row size");
    auto bigger=Choose(2000,1100,same(1));
    check(bigger.valid && bigger.cardHeight>one.cardHeight,"larger useful area allows a larger sparse preview");
    auto portraitOne=Choose(1538,864,same(1,.55));
    check(portraitOne.valid && portraitOne.cardHeight==one.cardHeight,"portrait single window retains height without distortion");
    auto wideOne=Choose(1538,864,same(1,3.5));
    check(wideOne.valid && wideOne.cardHeight<one.cardHeight && wideOne.width<=1538,"ultrawide single window is limited by width");
    auto small=Choose(700,430,same(6));
    check(small.valid && small.height<=430 && small.width<=700,"small display fits without scroll");
    check(small.cardHeight<six.cardHeight,"less available space reduces preview size");
    auto portrait=Choose(1538,864,same(6,.55));
    check(portrait.valid && portrait.cardHeight>=six.cardHeight,"portrait windows keep their useful height");
    std::vector<Item> mixed={{1.6,42,2},{.55,42,2},{2.6,42,2},{1,42,2},{1.6,42,2},{.75,42,2}};
    auto mix=Choose(1538,864,mixed);
    check(mix.valid && mix.width<=1538 && mix.height<=864,"mixed aspect ratios fit");
    check(!Choose(1000,800,{}).valid,"no fabricated layout for an empty list");
    check(!Choose(1000,800,same(129)).valid,"explicit sampling boundary");
    check(!Choose(-1,800,same(6)).valid,"invalid area rejected");
    check(!Choose(1000,std::numeric_limits<double>::infinity(),same(6)).valid,"unbounded height rejected");
    check(!Choose(1000,800,{{0,42,2}}).valid,"invalid aspect rejected");
    check(!Choose(1000,800,{{1.6,-1,2}}).valid,"invalid chrome rejected");
    check(!Choose(1000,800,same(6),std::numeric_limits<double>::quiet_NaN()).valid,"invalid ceiling rejected");
    check(!Choose(1000,800,same(6),-24).valid,"negative repair ceiling rejected");
    for(auto size:std::vector<std::pair<double,double>>{{1538,864},{1198,686},{910,540},{2000,1100}}){
        double previous=ComfortHeight(size.second);
        for(unsigned count=1;count<=64;++count){
            auto plan=Choose(size.first,size.second,same(count));
            check(plan.valid,"1..64 windows fit tested display sizes");
            if(!plan.valid)continue;
            check(plan.width<=size.first && plan.height<=size.second,"no model overflow");
            check(plan.cardHeight<=previous,"adding a same-shaped window does not increase preview size");
            check(plan.cardHeight<=size.second*.75,"display-relative comfortable upper bound");
            previous=plan.cardHeight;
        }
    }
    auto repair=Choose(1538,864,same(6),six.cardHeight-24);
    check(repair.valid && repair.cardHeight<six.cardHeight,"bounded overflow correction shrinks size");
    auto sparseRepair=Choose(1538,864,same(1),one.cardHeight-24);
    check(sparseRepair.valid && sparseRepair.cardHeight==one.cardHeight-24,"sparse overflow correction honors the same size ladder");
    for(auto size:std::vector<std::pair<double,double>>{{1538,864},{1198,686},{910,540},{2000,1100}}){
        std::vector<Item> variable;
        double previous=ComfortHeight(size.second);
        for(unsigned count=1;count<=64;++count){
            variable.push_back({std::vector<double>{1.6,.55,2.6,1,.75}[(count-1)%5],42,2});
            auto plan=Choose(size.first,size.second,variable);
            check(plan.valid,"mixed 1..64 windows fit tested areas");
            if(!plan.valid)continue;
            check(plan.width<=size.first && plan.height<=size.second,"mixed list remains inside available area");
            check(plan.cardHeight<=previous,"adding a mixed-aspect window does not enlarge the list");
            auto next=AtHeight(size.first,size.second,variable,plan.cardHeight+24);
            check(plan.cardHeight+24>ComfortHeight(size.second) || !next.valid,"chosen size is the largest fitting ladder step");
            previous=plan.cardHeight;
        }
    }
    std::cout<<"checks="<<checks<<" failures="<<failures<<'\n';
    return failures?1:0;
}
