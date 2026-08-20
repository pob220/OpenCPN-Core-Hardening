/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 ***************************************************************************/

#ifndef MODEL_EXTERNAL_API_H_
#define MODEL_EXTERNAL_API_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

#include "model/external_control.h"

namespace ocpn::control {

struct HttpRequest {
  std::string method;
  std::string path;
  std::map<std::string, std::string> headers;
  std::string body;
  std::string remote_address;
};

struct HttpResponse {
  int status = 500;
  std::map<std::string, std::string> headers;
  std::string body;
};

class TokenAuthorizer {
public:
  struct Principal {
    std::string id;
    std::set<std::string> scopes;
  };

  void Put(std::string token, Principal principal);
  void PutDigest(std::string token_sha256, Principal principal);
  void Revoke(const std::string& token);
  std::optional<Principal> Authenticate(const std::string& token) const;

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Principal> tokens_;
};

class ExternalApiRouter {
public:
  struct Options {
    bool enabled = false;
    std::size_t maximum_body_bytes = 1024 * 1024;
    std::string server_version;
    std::string api_version = "2.0.0";
    bool allow_lan = false;
  };

  ExternalApiRouter(ServiceBundle services,
                    std::shared_ptr<TokenAuthorizer> authorizer,
                    Options options);

  HttpResponse Handle(const HttpRequest& request) const;
  /** Encode a bounded batch for an already-authenticated event connection. */
  HttpResponse ReadEvents(std::uint64_t after_sequence, std::size_t maximum,
                          std::uint32_t type_mask) const;
  /** Parse a WebSocket subscription control message into an event type mask. */
  Result<std::uint32_t> ParseEventSubscription(
      const std::string& message) const;
  void CloseEvents();
  void Shutdown();

private:
  HttpResponse HandleAuthenticated(const HttpRequest& request,
                                   const TokenAuthorizer::Principal& principal)
      const;
  HttpResponse HandleRouteCommand(
      const HttpRequest& request, const TokenAuthorizer::Principal& principal,
      const std::string& path) const;

  ServiceBundle services_;
  std::shared_ptr<TokenAuthorizer> authorizer_;
  Options options_;
  mutable std::mutex idempotency_mutex_;
  mutable std::unordered_map<std::string, std::pair<std::string, HttpResponse>>
      idempotency_results_;
};

}  // namespace ocpn::control

#endif  // MODEL_EXTERNAL_API_H_
