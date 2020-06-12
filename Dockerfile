#FROM debian:testing
FROM arm32v7/debian:testing

RUN echo "# log: " \
  && set -x \
  && echo "deb-src http://deb.debian.org/debian testing main" \
  | tee -a /etc/apt/sources.list \
  && cat /etc/apt/sources.list \
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
  && apt-get update \
  && apt-get install -y \
    fdisk \
  && sync

RUN echo "# log: devel d-i" \
  && set -x \
  && cd installer \
  && git remote add devel https://github.com/CrossStream/debian-installer \
  && echo git fetch devel \
  && echo git reset --hard remotes/devel/sandbox/rzr/master  \
  && cd - \
  && sync

RUN echo "# log: " \
  && set -x \
  && grep "LINUX_KERNEL_ABI" "installer/build/config/common" \
  && fakeroot make -C installer/build sources.list.udeb \
  && echo "deb http://deb.debian.org/debian experimental main/debian-installer" \
  | tee -a installer/build/sources.list.udeb \
  && cat installer/build/sources.list.udeb \
  && fakeroot make -C installer/build build_netboot LINUX_KERNEL_ABI='5.7.0-rc5' \
  && sync

RUN echo "# log: " \
  && find installer/build/dest \
  && sync

# docker build .

/usr/local/opt/d-i/src/d-i/installer/build/dest
