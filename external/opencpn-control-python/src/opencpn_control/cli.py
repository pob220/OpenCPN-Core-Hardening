from __future__ import annotations

import argparse
import json
import os
import sys

from .client import Client, OpenCPNError


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(prog="opencpnctl")
    result.add_argument("--url", default=os.getenv("OPENCPN_URL", "https://127.0.0.1:8443"))
    result.add_argument("--token", default=os.getenv("OPENCPN_TOKEN"))
    result.add_argument("--insecure", action="store_true",
                        help="accept a self-signed development certificate")
    commands = result.add_subparsers(dest="command", required=True)
    commands.add_parser("status")
    commands.add_parser("navigation")
    routes = commands.add_parser("routes").add_subparsers(dest="route_command", required=True)
    routes.add_parser("list")
    show = routes.add_parser("show"); show.add_argument("guid")
    validate = routes.add_parser("validate")
    validate.add_argument("guid"); validate.add_argument("--minimum-depth-m", type=float, required=True)
    validate.add_argument("--land-margin-nm", type=float, default=0.0)
    activate = routes.add_parser("activate"); activate.add_argument("guid")
    activate.add_argument("--waypoint-guid"); activate.add_argument("--confirm", action="store_true")
    commands.add_parser("deactivate").add_argument("--confirm", action="store_true")
    return result


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    if not args.token:
        print("OPENCPN_TOKEN or --token is required", file=sys.stderr)
        return 2
    client = Client(args.url, args.token, verify_tls=not args.insecure)
    try:
        if args.command == "status": output = client.status()
        elif args.command == "navigation": output = client.navigation()
        elif args.command == "deactivate":
            if not args.confirm: raise OpenCPNError(0, "confirmation_required", "use --confirm")
            output = client.deactivate_route()
        elif args.route_command == "list": output = client.list_routes()
        elif args.route_command == "show": output = client.get_route(args.guid)
        elif args.route_command == "validate":
            route = client.get_route(args.guid)
            geometry = [point["position"] for point in route["waypoints"]]
            output = client.validate_route(geometry, minimum_depth_m=args.minimum_depth_m,
                                           land_margin_nm=args.land_margin_nm)
        elif args.route_command == "activate":
            if not args.confirm: raise OpenCPNError(0, "confirmation_required", "use --confirm")
            output = client.activate_route(args.guid, waypoint_guid=args.waypoint_guid)
        print(json.dumps(output, indent=2, sort_keys=True))
        return 0
    except OpenCPNError as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

