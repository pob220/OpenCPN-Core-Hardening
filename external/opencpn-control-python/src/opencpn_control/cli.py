from __future__ import annotations

import argparse
import json
import os
import sys
import time

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
    events = commands.add_parser("events").add_subparsers(
        dest="events_command", required=True)
    watch_events = events.add_parser("watch")
    watch_events.add_argument("types", nargs="*")
    routes = commands.add_parser("routes").add_subparsers(dest="route_command", required=True)
    routes.add_parser("list")
    show = routes.add_parser("show"); show.add_argument("guid")
    validate = routes.add_parser("validate")
    validate.add_argument("guid"); validate.add_argument("--minimum-depth-m", type=float, required=True)
    validate.add_argument("--land-margin-nm", type=float, default=0.0)
    activate = routes.add_parser("activate"); activate.add_argument("guid")
    activate.add_argument("--waypoint-guid"); activate.add_argument("--confirm", action="store_true")
    commands.add_parser("deactivate").add_argument("--confirm", action="store_true")
    planning = commands.add_parser("planning").add_subparsers(
        dest="planning_command", required=True)
    submit = planning.add_parser("submit")
    submit.add_argument("scenario", help="JSON planning request")
    watch = planning.add_parser("watch")
    watch.add_argument("job_id")
    watch.add_argument("--interval", type=float, default=1.0)
    planning.add_parser("cancel").add_argument("job_id")
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
        elif args.command == "events":
            for event in client.events(args.types or None):
                print(json.dumps(event, sort_keys=True), flush=True)
            return 0
        elif args.command == "deactivate":
            if not args.confirm: raise OpenCPNError(0, "confirmation_required", "use --confirm")
            output = client.deactivate_route()
        elif args.command == "planning":
            if args.planning_command == "submit":
                with open(args.scenario, encoding="utf-8") as scenario_file:
                    output = client.start_plan(json.load(scenario_file))
            elif args.planning_command == "cancel":
                output = client.cancel_plan(args.job_id)
            else:
                while True:
                    output = client.get_plan(args.job_id)
                    if output["state"] in {"completed", "failed", "cancelled"}:
                        if output["state"] == "completed":
                            output = {"job": output,
                                      "result": client.get_plan_result(args.job_id)}
                        break
                    time.sleep(max(0.1, args.interval))
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
