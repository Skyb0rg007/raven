#include "handle_channel.h"
#include "event.h"
#include "objects.h"
#include "child_process.h"

#include <array>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

static void check(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

static void send_event(int descriptor, raven::EventObject &event, uint64_t handle)
{
	raven::InheritedHandle record;
	check(event.describe(record), "cannot describe transferred event");
	record.handle = handle;
	record.handle_access = 0x1f0003;
	const auto bytes = raven::serialize_inherited({record});
	iovec vector{const_cast<uint8_t *>(bytes.data()), bytes.size()};
	alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
	msghdr message{};
	message.msg_iov = &vector;
	message.msg_iovlen = 1;
	message.msg_control = control;
	message.msg_controllen = sizeof(control);
	auto *header = CMSG_FIRSTHDR(&message);
	header->cmsg_level = SOL_SOCKET;
	header->cmsg_type = SCM_RIGHTS;
	header->cmsg_len = CMSG_LEN(sizeof(int));
	std::memcpy(CMSG_DATA(header), &record.descriptor, sizeof(int));
	check(::sendmsg(descriptor, &message, MSG_NOSIGNAL) == ssize_t(bytes.size()), "cannot queue event transfer");
}

int main(int argc, char **argv)
{
	try {
		if (argc > 1 && std::strcmp(argv[1], "--run-process") == 0) {
			int descriptor = -1;
			for (int index = 2; index < argc; ++index)
				if (std::string_view(argv[index]).starts_with("--handle-channel-fd="))
					descriptor = std::stoi(argv[index] + std::strlen("--handle-channel-fd="));
			char released;
			check(descriptor >= 0 && ::read(0, &released, 1) == 1, "child did not receive release gate");
			raven::EventObject final(0, true);
			send_event(descriptor, final, 0x1010);
			return 0;
		}
		raven::NativeObjects objects;
		std::array<std::unique_ptr<raven::HandleChannel>, 3> receivers;
		std::array<int, 3> senders;
		for (size_t index = 0; index < receivers.size(); ++index) {
			int pair[2];
			check(!::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, pair), "cannot create test channel");
			receivers[index] = std::make_unique<raven::HandleChannel>(pair[0]);
			senders[index] = pair[1];
			raven::add_child_channel(receivers[index].get());
		}
		raven::enable_process_requests();
		const auto peer = raven::process_request_channel(::getpid());
		check(bool(peer), "cannot connect process request peer");
		raven::receive_handles(objects);
		for (size_t round = 0; round < 1000; ++round)
			raven::receive_handles(objects);
		if (argc == 1 || std::strcmp(argv[1], "--idle")) {
			raven::EventObject first(0, true), second(0, true), final(0, true);
			send_event(senders[1], first, 0x1000);
			send_event(senders[2], second, 0x1004);
			raven::receive_handles(objects);
			check(objects.signaled(0x1000, 1) == 1 && objects.signaled(0x1004, 1) == 1, "simultaneous channels lost transferred handles");
			check(!objects.set_event(0x1000, false).status && !first.signaled(1), "event transfer copied rather than shared state");
			// A peer may close with both a malformed packet and a final valid transfer still queued.
			check(::send(senders[0], "x", 1, MSG_NOSIGNAL) == 1, "cannot queue malformed packet");
			send_event(senders[0], final, 0x1008);
			::close(senders[0]);
			senders[0] = -1;
			raven::receive_handles(objects);
			check(objects.signaled(0x1008, 1) == 1, "closed peer lost final queued transfer");
			bool nested = false;
			objects.process_request = [&](const raven::InheritedHandle &) {
				nested = true;
				raven::receive_handles(objects);
				return raven::InheritedHandle{};
			};
			raven::InheritedHandle request;
			request.kind = "process:test";
			const auto bytes = raven::serialize_inherited({request});
			check(::send(senders[1], bytes.data(), bytes.size(), MSG_NOSIGNAL) == ssize_t(bytes.size()), "cannot queue nested request");
			raven::receive_handles(objects);
			check(nested, "channel request did not serve nested IPC");
			for (size_t round = 0; round < 10; ++round)
				raven::receive_handles(objects);
			int gate[2];
			check(!::pipe2(gate, O_CLOEXEC), "cannot create child release gate");
			raven::ChildLaunch launch;
			launch.image = "ipc-unit.exe";
			launch.standard[0] = gate[0];
			uint32_t error = 0;
			auto child = raven::spawn_child(launch, error);
			check(bool(child), "cannot start final-transfer child");
			::close(gate[0]);
			// The child becomes readable after this sweep has already selected its channels.
			objects.process_request = [&](const raven::InheritedHandle &) {
				check(::write(gate[1], "x", 1) == 1, "cannot release child");
				const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
				while (!child->exited() && std::chrono::steady_clock::now() < end)
					std::this_thread::yield();
				check(child->exited() && child->exit_code() == 0, "final-transfer child did not finish");
				child.reset();
				return raven::InheritedHandle{};
			};
			check(::send(senders[1], bytes.data(), bytes.size(), MSG_NOSIGNAL) == ssize_t(bytes.size()), "cannot queue child release request");
			raven::receive_handles(objects);
			::close(gate[1]);
			check(objects.signaled(0x1010, 1) == 1, "reaping child lost its final transfer");
		}
		for (size_t index = 0; index < receivers.size(); ++index) {
			raven::remove_child_channel(receivers[index].get());
			if (senders[index] >= 0)
				::close(senders[index]);
		}
		std::cout << "handle channel contract passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
