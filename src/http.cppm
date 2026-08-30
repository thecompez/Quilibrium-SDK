export module quilibrium.http;
export import quilibrium.core;

export namespace quilibrium::http {
using method = quilibrium::http_method;
using headers = quilibrium::http_headers;
using request = quilibrium::http_request;
using response = quilibrium::http_response;
using transport = quilibrium::http_transport;
using transport_ptr = quilibrium::http_transport_ptr;
using quilibrium::http_method_name;
} // namespace quilibrium::http
