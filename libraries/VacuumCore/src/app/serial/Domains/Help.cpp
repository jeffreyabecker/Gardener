#include "app/serial/Domain.h"

#include "app/serial/Domains.h"

namespace gardener {
namespace {

// The domain list is the interface: every page it serves is read from a domain that is in it, so a
// topic taken from `help` is always one the named domain accepts.
class HelpDomain : public Domain {
 public:
  HelpDomain(DomainContext& ctx, DomainList& domains) : ctx_(ctx), domains_(domains) {}

  const char* name() const override { return "help"; }

  void execute(Stream& input) override {
    String line = input.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) {
      writeDomainList();
      return;
    }

    const int space = line.indexOf(' ');
    String domainName = space < 0 ? line : line.substring(0, space);
    String rest = space < 0 ? String() : line.substring(space + 1);
    rest.trim();
    domainName.toLowerCase();

    Domain* target = domains_.find(domainName.c_str());
    if (!target) {
      String detail;
      addItem(detail, "token", domainName.c_str());
      replyErr(ctx_.out, "help", domainName.c_str(), "unknown_domain", detail);
      return;
    }

    if (rest.length() == 0) {
      target->help(nullptr);
      return;
    }

    const int topicEnd = rest.indexOf(' ');
    String topic = topicEnd < 0 ? rest : rest.substring(0, topicEnd);
    String extra = topicEnd < 0 ? String() : rest.substring(topicEnd + 1);
    extra.trim();
    if (extra.length() > 0) {
      replyBadArgument(ctx_.out, "help", domainName.c_str(), extra.c_str());
      return;
    }

    topic.toLowerCase();
    target->help(topic.c_str());
  }

  void describe() override {
    String row;
    addItem(row, "domain", "help");
    addItemU32(row, "topics", 1);
    addItem(row, "about", "this help");
    replyRow(ctx_.out, row);
  }

  void help(const char* topic) override {
    if (!topic) {
      String header;
      addItem(header, "body", "rows");
      addItemU32(header, "rows", 1);
      replyOk(ctx_.out, "help", "help", header);

      String row;
      addItem(row, "topic", "help");
      addItem(row, "args", "[domain [topic]]");
      addItem(row, "about", "list domains, a domain's topics, or one topic");
      replyRow(ctx_.out, row);

      replyEnd(ctx_.out, "help", "help", header);
      return;
    }

    if (String(topic).equalsIgnoreCase("help")) {
      String header;
      addItem(header, "topic", "help");
      addItem(header, "body", "rows");
      addItemU32(header, "rows", 3);
      replyOk(ctx_.out, "help", "help", header);

      String domain;
      addItem(domain, "arg", "domain");
      addItem(domain, "type", "text");
      replyRow(ctx_.out, domain);

      String topicArg;
      addItem(topicArg, "arg", "topic");
      addItem(topicArg, "type", "text");
      replyRow(ctx_.out, topicArg);

      String reply;
      addItem(reply, "reply", "domains|topics");
      replyRow(ctx_.out, reply);

      replyEnd(ctx_.out, "help", "help", header);
      return;
    }

    replyUnknownTopic(ctx_.out, "help", topic);
  }

 private:
  void writeDomainList() {
    const uint8_t count = domains_.count();
    String header;
    addItem(header, "body", "rows");
    addItemU32(header, "rows", count);
    replyOk(ctx_.out, "help", "domains", header);

    for (uint8_t i = 0; i < count; i++) {
      Domain* domain = domains_.at(i);
      if (domain) domain->describe();
    }

    replyEnd(ctx_.out, "help", "domains", header);
  }

  DomainContext& ctx_;
  DomainList& domains_;
};

}  // namespace

Domain* createHelpDomain(DomainContext& ctx, DomainList& domains) {
  return new HelpDomain(ctx, domains);
}

}  // namespace gardener


