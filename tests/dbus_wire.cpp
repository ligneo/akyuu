/**
 * Akyuu
 * Copyright (C) 2026, cenky <cenkkgl@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <anisthesia/lin_dbus.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace anisthesia::lin::detail::dbus;
using bytes = std::vector<std::uint8_t>;

void require(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

// Writes values in either byte order, as a peer on the bus may
class Builder {
public:
  explicit Builder(std::endian order) : order_{order} {}

  Builder& align(std::size_t n) {
    data_.resize((data_.size() + n - 1) / n * n);
    return *this;
  }
  Builder& byte(std::uint8_t v) {
    data_.push_back(v);
    return *this;
  }
  Builder& u32(std::uint32_t v) {
    align(4);
    for (int i = 0; i < 4; ++i) {
      const int shift = order_ == std::endian::little ? 8 * i : 24 - 8 * i;
      data_.push_back(static_cast<std::uint8_t>(v >> shift));
    }
    return *this;
  }
  Builder& str(std::string_view v) {
    u32(static_cast<std::uint32_t>(v.size()));
    data_.insert(data_.end(), v.begin(), v.end());
    return byte(0);
  }
  Builder& sig(std::string_view v) {
    byte(static_cast<std::uint8_t>(v.size()));
    data_.insert(data_.end(), v.begin(), v.end());
    return byte(0);
  }
  bytes take() { return std::move(data_); }

private:
  std::endian order_;
  bytes data_;
};

// A method return for `reply_serial`, whose body is `as` with the given names
bytes makeReply(std::endian order, std::uint32_t reply_serial,
                const std::vector<std::string>& names) {
  Builder names_builder{order};
  for (const auto& name : names) names_builder.str(name);
  const auto array = names_builder.take();

  Builder body{order};
  body.u32(static_cast<std::uint32_t>(array.size()));
  auto body_bytes = body.take();
  body_bytes.insert(body_bytes.end(), array.begin(), array.end());

  Builder fields{order};
  fields.byte(5).sig("u").u32(reply_serial);
  fields.align(8).byte(8).sig("g").sig("as");
  const auto fields_bytes = fields.take();

  Builder message{order};
  message.byte(order == std::endian::little ? 'l' : 'B').byte(2).byte(0).byte(1);
  message.u32(static_cast<std::uint32_t>(body_bytes.size()));
  message.u32(99);
  message.u32(static_cast<std::uint32_t>(fields_bytes.size()));
  auto result = message.take();
  result.insert(result.end(), fields_bytes.begin(), fields_bytes.end());
  result.resize((result.size() + 7) / 8 * 8);
  result.insert(result.end(), body_bytes.begin(), body_bytes.end());
  return result;
}

void testReplies(std::endian order) {
  const auto data = makeReply(order, 7, {"org.mpris.MediaPlayer2.mpv", ":1.5"});

  require(MeasureMessage(bytes_t{data}.first(15)) == 0, "a partial header needs more data");
  require(MeasureMessage(data) == data.size(), "the whole message is measured");

  const auto message = ParseMessage(data);
  require(message.has_value(), "a reply parses");
  require(message->type == MessageType::MethodReturn, "the type is read");
  require(message->reply_serial == 7, "the reply serial is read");
  require(message->signature == "as", "the signature is read");

  Reader reader{message->body, message->order};
  std::size_t end = 0;
  std::string_view name;
  require(reader.EnterArray(4, end), "the array opens");
  require(reader.ReadString(name) && name == "org.mpris.MediaPlayer2.mpv", "first string");
  require(reader.ReadString(name) && name == ":1.5", "second string");
  require(reader.position() == end, "the array ends after its last string");

  for (std::size_t size = 16; size < data.size(); ++size) {
    require(!ParseMessage(bytes_t{data}.first(size)), "a truncated message is rejected");
  }
}

void testMarshalledCall() {
  const Call call{.destination = "org.mpris.MediaPlayer2.mpv",
                  .path = "/org/mpris/MediaPlayer2",
                  .interface = "org.freedesktop.DBus.Properties",
                  .member = "GetAll",
                  .argument = "org.mpris.MediaPlayer2.Player"};
  const auto data = MarshalCall(call, 42);

  require(MeasureMessage(data) == data.size(), "a call is measured to its full size");
  const auto message = ParseMessage(data);
  require(message.has_value(), "our own call parses");
  require(message->type == MessageType::MethodCall, "a call has the right type");
  require(message->signature == "s", "the argument is declared");

  Reader reader{message->body, message->order};
  std::string_view argument;
  require(reader.ReadString(argument) && argument == call.argument, "the argument is written");
}

void testMalformedHeaders() {
  auto data = makeReply(std::endian::little, 1, {"x"});

  auto bad_order = data;
  bad_order[0] = 'x';
  require(!MeasureMessage(bad_order), "an unknown byte order is fatal");

  auto bad_version = data;
  bad_version[3] = 2;
  require(!MeasureMessage(bad_version), "an unknown protocol version is fatal");

  auto huge = data;
  huge[7] = 0x7f;
  require(!MeasureMessage(huge), "a body beyond the limit is fatal");
}

void testSkipping() {
  // Each value is preceded by one byte, so that its padding matters
  Builder builder{std::endian::little};
  builder.byte(1);                            // y
  builder.align(8).u32(1).u32(2);             // t
  builder.str("title");                       // s
  builder.sig("as").u32(0).align(4);          // v holding an empty as
  builder.sig("d").align(8).u32(0).u32(0);    // v holding a d
  builder.u32(0).align(8);                    // a{sv}, empty, padded to its first element
  builder.align(8).u32(9).str("nested");      // (is)
  const auto data = builder.take();

  Reader reader{data, std::endian::little};
  require(reader.Skip("ytsvva{sv}(is)"), "every type is skipped");
  require(reader.position() == data.size(), "skipping ends after the last value");

  Reader bad_type{data, std::endian::little};
  require(!bad_type.Skip("yz"), "an unknown type fails");

  Reader unbalanced{data, std::endian::little};
  require(!unbalanced.Skip("(y"), "an unbalanced struct fails");

  Reader lone_array{data, std::endian::little};
  require(!lone_array.Skip("a"), "an array needs an element type");

  const bytes huge_array{0, 0, 0, 0x7f};
  Reader beyond_limit{huge_array, std::endian::little};
  require(!beyond_limit.Skip("ay"), "an array beyond the limit fails");

  const bytes unterminated{1, 0, 0, 0, 'a', 'b'};
  Reader no_terminator{unterminated, std::endian::little};
  require(!no_terminator.Skip("s"), "a string needs its terminator");

  // A failure is permanent, even if a later read would have fit
  Reader failed{data, std::endian::little};
  require(!failed.Skip("z"), "fails first");
  std::uint8_t value = 0;
  require(!failed.ReadByte(value), "stays failed");

  std::string nested(70, 'a');
  nested += 'y';
  const bytes zeros(64, 0);
  Reader too_deep{zeros, std::endian::little};
  require(!too_deep.Skip(nested), "nesting is limited");
}

void testArrays() {
  // The padding before the first element counts even though the length does not include it
  Builder aligned{std::endian::little};
  aligned.u32(8).align(8).u32(1).u32(2).byte(9);
  const auto aligned_bytes = aligned.take();
  Reader reader{aligned_bytes, std::endian::little};
  require(reader.Skip("aty"), "an aligned array is skipped");
  require(reader.position() == 17, "the padding before the elements is skipped");

  const std::uint32_t limit = 67'108'864;
  bytes at_limit(4 + limit + 8, 0);
  at_limit[3] = 0x04;  // little endian bytes of the limit
  at_limit[0] = 0;
  Reader accepted{at_limit, std::endian::little};
  require(accepted.Skip("ay"), "an array at the limit is accepted");

  at_limit[0] = 1;
  Reader rejected{at_limit, std::endian::little};
  require(!rejected.Skip("ay"), "an array just beyond the limit is rejected");
}

void testNestedVariants() {
  const auto nest = [](int depth) {
    bytes data;
    for (int i = 0; i < depth; ++i) {
      data.insert(data.end(), {1, 'v', 0});
    }
    data.insert(data.end(), {1, 'y', 0, 5});
    return data;
  };

  const auto shallow = nest(40);
  Reader allowed{shallow, std::endian::little};
  require(allowed.Skip("v"), "ordinary nesting is allowed");
  require(allowed.position() == shallow.size(), "nested variants are skipped in full");

  const auto deep = nest(80);
  Reader refused{deep, std::endian::little};
  require(!refused.Skip("v"), "variants cannot nest without limit");
}

void testAddresses() {
  const auto addresses = ParseAddresses(
      "tcp:host=localhost,port=1;unix:path=/run/user/1000/bus,guid=abc;"
      "unix:abstract=/tmp/dbus-xyz;unix:path=/tmp/a%20b;unix:path=/tmp/bad%2;unix:guid=only");

  require(addresses.size() == 3, "only usable Unix addresses are kept");
  require(!addresses[0].abstract && addresses[0].path == "/run/user/1000/bus", "path");
  require(addresses[1].abstract && addresses[1].path == "/tmp/dbus-xyz", "abstract");
  require(addresses[2].path == "/tmp/a b", "escapes are decoded");
  require(ParseAddresses("").empty(), "no addresses");
}

}  // namespace

int main() {
  testReplies(std::endian::little);
  testReplies(std::endian::big);
  testMarshalledCall();
  testMalformedHeaders();
  testSkipping();
  testArrays();
  testNestedVariants();
  testAddresses();
}
