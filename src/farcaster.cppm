module;
#include <cstdint>
#include <memory>

export module quilibrium.farcaster;
import quilibrium.core;

export namespace quilibrium::farcaster {
enum class write_operation : std::uint8_t { cast_add,cast_remove,reaction_add,reaction_remove,link_add,link_remove,user_data_add,verification_add,verification_remove };
struct signed_message final { write_operation operation{}; bytes protobuf_message{}; };
class writer { public: virtual ~writer()=default; [[nodiscard]] virtual task<status> submit(signed_message message,call_options options={})=0; };
using writer_ptr=std::shared_ptr<writer>;
} // namespace quilibrium::farcaster
