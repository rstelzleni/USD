#!/bin/csh

foreach f ( `find . -iname "schema.usda" | grep -v testenv | grep -v inst` )
 echo "Processing $f"
 pushd `dirname $f` > /dev/null
 p4 -q edit ...
 docker run -it --rm -v /home/ryans-np/trees/usd-dev/USD-rstelzleni/USD-inst-plussidefx-py36:/opt/USD-inst -v `pwd`:/opt/USD usd-build-env:centos /opt/USD-inst/bin/usdGenSchema -v
 #docker run -it --rm -v /home/ryans-np/trees/usd-dev/USD-rstelzleni/USD-inst-plussidefx-py36:/opt/USD-inst -v `pwd`:/opt/USD usd-build-env:centos touch test.test
 p4 -q revert -a ...
 popd > /dev/null
end
