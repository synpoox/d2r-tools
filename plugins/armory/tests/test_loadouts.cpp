#include "../src/store.h"
#include <cstdlib>
#include <iostream>
#include <chrono>
using namespace armory;
void Check(bool ok) { if (!ok) std::abort(); }
int main() {
    Fingerprint a{1}, b{2};
    Snapshot s{1,true,true,true,{{0,2,1}},{{1,a,{-1,11,0},1,1},{2,b,{0,0,0},1,1}}};
    auto partial = Capture(s, 1u << 10); Check(partial && partial->size()==1 && partial->at(11)==a);
    auto full = Capture(s, 0x0fff); Check(full && full->size()==12 && !full->at(12));
    Book book{"mod/character",{}};
    Check(Add(book,"Farm",*partial)); Check(Add(book,"Boss",*full));
    Check(!Add(book,"Farm",*full)); Check(!Rename(book,"Farm","Boss"));
    Check(Rename(book,"Farm","Magic Find")); Check(Active(book,s).size()==2);
    Check(Cycle(book,1,true)==0 && Cycle(book,0,false)==1);
    Check(Update(book,"Boss",{{11,b}})); Check(Active(book,s).size()==1);
    Check(!Update(book,"Boss",{{11,a},{12,a}}));
    auto bytes = Encode(book); Check(bytes && Decode(*bytes,book.scope)==book);
    Check(!Decode(*bytes,"other")); Check(!Decode(*bytes+"x",book.scope));
    for (size_t i=0; i<bytes->size(); ++i) Check(!Decode(std::string_view(*bytes).substr(0,i),book.scope));
    auto wrong = *bytes; wrong[4]=2; Check(!Decode(wrong,book.scope));
    s.items.push_back({3,a,{0,1,0},1,1}); Check(!Capture(s,1u<<10)); Check(Active(book,s).empty());
    auto dir=std::filesystem::temp_directory_path()/ ("armory-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(std::filesystem::create_directory(dir)); auto path=dir/"test.armory";
    Book current{book.scope,{}}; Check(CommitBook(path,current,book)==StoreResult::Ok && current==book);
    Book loaded; Check(ReadBook(path,book.scope,loaded)==StoreResult::Ok && loaded==book);
    auto next=book; Check(Erase(next,"Boss"));
    { std::ofstream lock(path.string()+".tmp"); lock<<"occupied"; }
    Check(CommitBook(path,current,next)==StoreResult::IoError && current==book);
    Check(ReadBook(path,book.scope,loaded)==StoreResult::Ok && loaded==book);
    std::filesystem::remove(path.string()+".tmp");
    Check(CommitBook(path,current,next)==StoreResult::Ok && current==next);
    Check(CommitBook(path,book,next)==StoreResult::Invalid);
    { std::ofstream corrupt(path,std::ios::binary|std::ios::trunc); corrupt<<"bad"; }
    Check(ReadBook(path,current.scope,loaded)==StoreResult::Invalid && loaded.entries.size()==2);
    Check(CommitBook(path,current,book)==StoreResult::Invalid && current==next);
    std::filesystem::remove_all(dir);
    std::cout<<"Armory loadout and store checks passed\n";
}
