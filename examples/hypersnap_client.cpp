#include <iostream>
#include <string>
import quilibrium.core;
import quilibrium.sdk;

int main(int argc,char** argv) {
    const std::uint64_t fid=argc>1?std::stoull(argv[1]):3;
    auto connected=quilibrium::connect();
    if(!connected){std::cerr<<connected.error().message<<'\n';return 1;}
    auto user=quilibrium::sync_wait(connected->hypersnap().users().get_by_fid(fid));
    if(!user){std::cerr<<user.error().message<<'\n';return 2;}
    std::cout<<user->display_name<<" (@"<<user->username<<") fid="<<user->fid<<'\n';
    return 0;
}
