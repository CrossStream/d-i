FROM debian:testing

RUN echo "# log: " \
  && set -x \
  && echo "deb-src http://deb.debian.org/debian testing main" \
  | tee -a /etc/apt/sources.list \
  && apt-get update \
  && apt-get build-dep debian-installer -y \
  && sync

ENV workdir /usr/local/opt/d-i/src/d-i
ADD . ${workdir}
WORKDIR ${workdir}

RUN echo "# log: " \
  && set -x \
  && apt-get update \
  && apt-get install -y \
    fakeroot \
    fdisk \    
    git \
    make \
    myrepos \
  && sync

RUN echo "# log: " \
  && set -x \
  && ./scripts/git-setup ||: \
  && mr checkout \
  && sync

RUN echo "# log: " \
  && set -x \
  && grep "LINUX_KERNEL_ABI" "installer/build/config/common" \
  && fakeroot make -C installer/build sources.list.udeb \
  && cat installer/build/sources.list.udeb \
  && fakeroot make -C installer/build build_netboot \
  && sync
