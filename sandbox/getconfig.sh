#!/bin/sh

for dev in $@; do
    iotsa --target $dev --protocol hps allInfo > config/$dev.config
done

