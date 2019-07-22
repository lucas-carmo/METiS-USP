% Teste pra implementar JONSWAP no METiS

clear all
close all
clc

% Simulation parameters
Tmax = 3600;
dt = 0.25;
t = 0:dt:Tmax;


% JONSWAP parameters
Hs = 2;
Tp = 11;
gamma = 3.3;
dir = 45;
wlow = 0.314159;
whigh = 1.570596;



%========================================================================
dw = 2*pi/Tmax;
wmax = pi/dt;


% Calcula o Jonswap
w = 0.01:dw:wmax;
wp = 2*pi/Tp;

sigma = 0.09*ones(size(w));
sigma(w <= wp) = 0.07;
A = exp( -0.5 * ((w/wp-1)./sigma).^2 );

Sw = 0.3125 * Hs^2 * Tp * (w/wp).^(-5) .* exp(-1.25 * (wp./w).^4 ) .* (1-0.287*log(gamma)) .* gamma.^A;

figure('color', 'w')
plot(w,Sw)


% Calcula as amplitudes
amp = 0 * (wlow:dw:whigh);
omega = zeros(size(amp));
Nwaves = length(amp);

ii = 1;
for wloop = wlow : dw : whigh    
    if wloop <= wp
        sigma = 0.07;
    else
        sigma = 0.09;
    end
    A = exp( -0.5 * ((wloop/wp-1)/sigma)^2 );
    S = 0.3125 * Hs^2 * Tp * (wloop/wp)^(-5) * exp(-1.25 * (wp/wloop).^4 ) * (1-0.287*log(gamma)) * gamma^A;
    omega(ii) = wloop;
    amp(ii) = sqrt(2 * S * dw);    
    ii = ii + 1;
end

% Phase is a randon number between -pi and pi
phase = -pi + (pi + pi) * rand(size(amp));


y = zeros(size(t));
for ii = 1:length(amp)
    y = y + amp(ii)*cos(omega(ii)*t + phase(ii));
end

figure('color', 'w')
plot(t, y)


% Testar se bate com o espectro de entrada
[Sres, w2] = pwelch(y,[],[],[],1/dt);  
Sres = Sres/(2*pi); % passar o espectro para rad/s 
w2 = 2*pi*w2;

figure('color', 'w')
plot(w2,Sres, 'b')
hold on
plot(w,Sw, 'r')
legend('From time series', 'JONSWAP')
xlim([0 2*whigh])

hold on
plot([wlow wlow], [0 max(1.25*Sw)], '--k')
hold on
plot([whigh whigh], [0 max(1.25*Sw)], '--k')

