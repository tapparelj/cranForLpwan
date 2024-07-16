% clear
clc,basic_init
close all
for idx = [0,999]
    try
        f = fopen(["tmp"+string(idx)+".txt"]);

        samp = fread(f,'float');
        fclose(f);
        samp = samp(1:2:end)+1j*samp(2:2:end) ;% 526
                if idx==999
                samp = samp(abs(4707):end)
                end
        %         if(sum((diff(samp)~=-19)))
        %         idx
        %         figure(1)
        %         plot(((samp-samp(1)))),hold on
        %         stem(((diff(samp)))),hold on
        %         end
        thresh = 1e-4;
        [~,start]=max((abs(diff(abs(samp)>1*thresh))));
        [~,end_]=max((abs(diff(abs(flip(samp))>1*thresh))));
        end_= numel(samp)-end_;
%         if((end_-start)<9760)
            figure(1)
            subplot(3,1,1)
            plot(abs(samp)),hold on
            title([idx "magnitude, frame size:" end_-start])
            subplot(3,1,2)
            freq(samp),hold on
            title("instant frequency")
            subplot(3,1,3)
            stem(20*log10(abs(fftshift(fft(samp(1:end))))),'.'),hold on
            tilte("spectrum")
%         end
    catch
    end
end

%%
f = fopen(['file_sink.bin']);
samp = fread(f,'uint8');
fclose(f);
figure
plot(samp)
%% load log from frame_sync block

f = fopen(['<log file name>']);
val = fread(f,'float');
fclose(f);

snr_est = val(1:5:end);
cfo_est = val(2:5:end);
sto_est = val(3:5:end);
sfo_est = val(4:5:end);
off_by_one_est = val(5:5:end);
figure
subplot(2,2,1)
plot(snr),hold on
plot([0 numel(snr)],[mean(snr),mean(snr)])
title('snr')
grid on
subplot(2,2,2)
plot(cfo),hold on
plot([0 numel(cfo)],[mean(cfo),mean(cfo)])
title('cfo')
grid on
subplot(2,2,3)
plot(sfo),hold on
plot([0 numel(sfo)],[mean(sfo),mean(sfo)])
title('sfo')
grid on
subplot(2,2,4)
plot(off_by_one)
title('off by one')
grid on
%%
edge=diff(abs(samp));
[a,b]=maxk(abs(edge),4)
diff(sort(b))
%%
% len=(501+12.25+4+4)*128;
len = 300;
% samp=samp(1:len*floor(length(samp)/len));
samp = reshape(samp,len,length(samp)/len);
% samp(1:4,end-3:end)
% figure(1)
% plot(samp(:,:)),hold on
% plot(samp(:,500)),hold on
% plot(samp(:,end))
% figure(2)

% t=sum(samp(:,1)~=samp);
% plot(t)
% disp("diff 1st-last: "+ int2str( sum(samp(:,1)~=samp(:,2))))
plot((samp(:,:)))


%%



%%
% samp = samp(1:2:end)+1j*samp(2:2:end) ;% 526

%
% len = 24;

len = 300
samp=samp(1:len*floor(length(samp)/len));
samp=reshape(samp,len,length(samp)/len);
% samp(1:4,end-3:end)
figure(1)

plot((((samp(:,1))))),hold on
plot((((samp(:,1218))))),hold on
%
% figure(1)
% plot(samp(:,259))
figure(3)
t=sum(samp(:,1)~=samp)
plot(t),hold on
% disp("diff 1st-last: "+ int2str( sum(samp(:,1)~=samp(:,2))))
%%
% close all
f = fopen(['../tmp1.txt']);
samp = fread(f,'float');
fclose(f);

samp = samp(1:2:end)+1j*samp(2:2:end) ;
n = (300+12.25+10)*128*4
samp = samp(1:floor(length(samp)/n)*n);

samp=reshape(samp,n,length(samp)/n);

figure(4)
% plot(diff(unwrap(angle(samp(1568+128:end))))),hold on
% legend('hat','usrp')
plot(diff(unwrap(angle((samp)))))
% plot(abs(samp))
%%
(300+12.25+1)*128*4


%%

len=(20+12.25+20)*2^7;
samp=samp(1:len*floor(length(samp)/len));
samp=reshape(samp,len,length(samp)/len);

% samp(1:4,end-3:end)
figure(1)

plot(diff(unwrap(angle(samp(:,1))))),hold on
plot(diff(unwrap(angle(samp(:,1825))))),hold on
% plot(diff(unwrap(angle(samp(:,33))))),hold on

figure(2)
t=sum(samp(:,1)~=samp);
plot(t)
% disp("diff 1st-last: "+ int2str( sum(samp(:,1)~=samp(:,2))))


%% result
f = fopen(['./rec_pay']);
samp = fread(f,'uint8');
fclose(f);
%%
ref ='One morning, when Gregor Samsa woke from troubled dreams, he found himself transformed in his bed into a horrible vermin. He lay on his armour-like back, and if he lifted his head a little he could see his brown belly, slightly domed and divided by arches';

ref=double(ref).';
len = 255;
samp=reshape(samp,len,length(samp)/len);

disp("errors: "+num2str( sum(sum(samp~=ref))))

%% frame len
clc
h=0;
crc=0;
pl=23;
sf=7;
cr=0;

fs=125000;

payload_symbol = 8 + max(0, ceil((20*h+8*pl+16*crc-4*sf+8)/(4*sf))*(cr+4));
duration_s = (12.25+payload_symbol)*2^sf/fs;
display(payload_symbol)
display(duration_s)


%%

f = fopen(['sx1276_sensi_files/sf8_bw250_cr4_paylen10_Nframes1000-97dBm']);
samp = fread(f,'char');
fclose(f);
figure
plot(samp)
title(sum(samp))

%% plot real
clear,clc,close
samp = dlmread("debug.txt");
samp = samp(:,1:end-1);
figure
samp=samp.';
% for i=30:500
stem(samp(:))
grid on
% [~,maxid]=max(samp(:,i));
% title(maxid-1)
% waitforbuttonpress
% end
% xlim([1,1e5])

%% plot complex
% clear,clc
samp = dlmread("debug_fft_demod.txt");
samp = samp(:,1:end-1);
samp_c = samp(:,1:2:end)+1i*samp(:,2:2:end);
samp_c = samp_c.';
sf=7

% samp_c=samp_c(1:4:end)

down = lora_symb(sf,1,[-1]);

%CFO
cfo = 8.2;
down = down.* exp(1i*2*pi*[1:length(down)].'*-cfo/(2^sf));


figure(1)
clf
% samp_c=reshape(samp_c,128,[]);
% plot(fft(samp_c(:,1:8).*down))
inst_freq(samp_c)
samp_c = [samp_c;zeros((2^sf*.75),1)];
 
for i = 1:13
    subplot(2,1,1)
    stem(abs(fft(samp_c((i-1)*2^sf+1:i*2^sf,1).*(down(:)))))
    
    subplot(2,1,2)
    inst_freq(samp_c((i-1)*2^sf+1:i*2^sf,1))
    title(i)
    waitforbuttonpress
end

stem(abs(xcorr(ref_pre,(samp_c))))

title(abs(ref_pre'*samp_c))


% plot((reshape(samp_c,128,[])))
% 
% xlim([1,1e5])

%
%%
figure;
plot(unwrap(angle(ref_pre))),hold on,grid on
plot(unwrap(angle(samp_c)))
xlabel('sample')
ylabel('phase')
legend("reference preamble","synchronized preamble")
xlim([1,2^sf*(8+2+2.25)])


%% plot complex fft_demod
samp = dlmread("debug_fft_demod.txt");
samp = samp(:,1:end-1);
samp_c = samp(:,1:2:end)+1i*samp(:,2:2:end);
samp_c = samp_c.';
figure;
subplot(2,1,1)
inst_freq(samp_c(:)),hold on
subplot(2,1,2)
plot(real(samp_c(:))),hold on

