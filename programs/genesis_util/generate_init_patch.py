#!/usr/bin/env python3

import argparse
import json
import subprocess
import sys

def dump_json(obj, out, pretty):
    if pretty:
        json.dump(obj, out, indent=2, sort_keys=True)
    else:
        json.dump(obj, out, separators=(",", ":"), sort_keys=True)
    return

def main():
    parser = argparse.ArgumentParser(description="Generate a patch file that adds init accounts")
    parser.add_argument("-o", "--output", metavar="OUT", default="-", help="output filename (default: stdout)")
    parser.add_argument("-n", "--num", metavar="N", default=11, type=int, help="number of init validators")
    parser.add_argument("-p", "--pretty", action="store_true", default=False, help="pretty print output")
    parser.add_argument("-s", "--secret", metavar="SECRET", default=None, help="private key generation secret")
    opts = parser.parse_args()

    if opts.secret is None:
        sys.stderr.write("missing required parameter --secret\n")
        sys.stderr.flush()
        sys.exit(1)

    wit_accounts = []
    wit_wits = []
    council = []

    for i in range(opts.num):
        owner_str = subprocess.check_output(["programs/genesis_util/get_dev_key", opts.secret, "wit-owner-"+str(i)]).decode("utf-8")
        active_str = subprocess.check_output(["programs/genesis_util/get_dev_key", opts.secret, "wit-active-"+str(i)]).decode("utf-8")
        prod_str = subprocess.check_output(["programs/genesis_util/get_dev_key", opts.secret, "wit-block-signing-"+str(i)]).decode("utf-8")
        owner = json.loads(owner_str)
        active = json.loads(active_str)
        prod = json.loads(prod_str)
        wit_accounts.append({
            "name" : "init"+str(i),
            "owner_key" : owner[0]["public_key"],
            "active_key" : active[0]["public_key"],
            "is_lifetime_member" : True,
            })
        wit_wits.append({
            "owner_name" : "init"+str(i),
            "block_producer_key" : prod[0]["public_key"],
            })
        council.append({"owner_name" : "init"+str(i)})
    result = {
       "append" : {
       "initial_accounts" : wit_accounts },
       "replace" : {
       "initial_block_producers" : opts.num,
       "initial_worker_candidates" : [],
       "initial_validator_candidates" : wit_wits,
       "initial_delegate_candidates" : council,
        }
    }

    if opts.output == "-":
        dump_json( result, sys.stdout, opts.pretty )
        sys.stdout.flush()
    else:
        with open(opts.output, "w") as f:
            dump_json( result, f, opts.pretty )
  
    return

if __name__ == "__main__":
    main()
