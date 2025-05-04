#!/bin/sh

for dev in $@; do
    iotsa --target $dev --protocol hps --verbose allInfo > config/$dev.config
done

